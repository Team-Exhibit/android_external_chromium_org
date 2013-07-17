// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_adb_bridge.h"

#include <map>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/adb/android_rsa.h"
#include "chrome/browser/devtools/adb/android_usb_device.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/tethering_adb_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/server/web_socket.h"

using content::BrowserThread;
using net::WebSocket;

namespace {

static const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";
static const char kHostDevicesCommand[] = "host:devices";
static const char kHostTransportCommand[] = "host:transport:%s|%s";
static const char kLocalAbstractCommand[] = "localabstract:%s";
static const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
static const char kUnknownModel[] = "Unknown";
static const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";

static const char kPageListRequest[] = "GET /json HTTP/1.1\r\n\r\n";
static const char kVersionRequest[] = "GET /json/version HTTP/1.1\r\n\r\n";
static const char kWebSocketUpgradeRequest[] = "GET %s HTTP/1.1\r\n"
    "Upgrade: WebSocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n";
const int kAdbPort = 5037;
const int kBufferSize = 16 * 1024;

typedef DevToolsAdbBridge::Callback Callback;
typedef DevToolsAdbBridge::PagesCallback PagesCallback;
typedef std::vector<scoped_refptr<DevToolsAdbBridge::AndroidDevice> >
    AndroidDevices;
typedef base::Callback<void(const AndroidDevices&)> AndroidDevicesCallback;

class AdbDeviceImpl : public DevToolsAdbBridge::AndroidDevice {
 public:
  explicit AdbDeviceImpl(const std::string& serial)
      : AndroidDevice(serial) {
  }

  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE {
    std::string query = base::StringPrintf(kHostTransportCommand,
                                           serial().c_str(), command.c_str());
    AdbClientSocket::AdbQuery(kAdbPort, query, callback);
  }

  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE {
    std::string socket_name =
        base::StringPrintf(kLocalAbstractCommand, name.c_str());
    AdbClientSocket::TransportQuery(kAdbPort, serial(), socket_name, callback);
  }
 private:
  virtual ~AdbDeviceImpl() {}
};

class UsbDeviceImpl : public DevToolsAdbBridge::AndroidDevice {
 public:
  explicit UsbDeviceImpl(AndroidUsbDevice* device)
      : AndroidDevice(device->serial()),
        device_(device) {
  }

  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE {
    net::StreamSocket* socket = device_->CreateSocket(command);
    int result = socket->Connect(base::Bind(&UsbDeviceImpl::OpenedForCommand,
                                            this, callback, socket));
    if (result != net::ERR_IO_PENDING)
      callback.Run(result, std::string());
  }

  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE {
    std::string socket_name =
        base::StringPrintf(kLocalAbstractCommand, name.c_str());
    net::StreamSocket* socket = device_->CreateSocket(socket_name);
    int result = socket->Connect(base::Bind(&UsbDeviceImpl::OnOpenSocket, this,
                                            callback, socket));
    if (result != net::ERR_IO_PENDING)
      callback.Run(result, NULL);
  }

 private:
  void OnOpenSocket(const SocketCallback& callback,
                    net::StreamSocket* socket,
                    int result) {
    callback.Run(result, result == net::OK ? socket : NULL);
  }

  void OpenedForCommand(const CommandCallback& callback,
                        net::StreamSocket* socket,
                        int result) {
    if (result != net::OK) {
      callback.Run(result, std::string());
      return;
    }
    scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kBufferSize);
    result = socket->Read(buffer, kBufferSize,
                          base::Bind(&UsbDeviceImpl::OnRead, this,
                                     socket, buffer, std::string(), callback));
    if (result != net::ERR_IO_PENDING)
      OnRead(socket, buffer, std::string(), callback, result);
  }

  void OnRead(net::StreamSocket* socket,
              scoped_refptr<net::IOBuffer> buffer,
              const std::string& data,
              const CommandCallback& callback,
              int result) {
    if (result <= 0) {
      callback.Run(result, result == 0 ? data : std::string());
      delete socket;
      return;
    }

    std::string new_data = data + std::string(buffer->data(), result);
    result = socket->Read(buffer, kBufferSize,
                          base::Bind(&UsbDeviceImpl::OnRead, this,
                                     socket, buffer, new_data, callback));
    if (result != net::ERR_IO_PENDING)
      OnRead(socket, buffer, new_data, callback, result);
  }

  virtual ~UsbDeviceImpl() {}
  scoped_refptr<AndroidUsbDevice> device_;
};

class AdbQueryCommand : public base::RefCounted<AdbQueryCommand> {
 public:
  AdbQueryCommand(const std::string& query,
                  const Callback& callback)
      : query_(query),
        callback_(callback) {
  }

  void Run() {
    AdbClientSocket::AdbQuery(kAdbPort, query_,
                              base::Bind(&AdbQueryCommand::Handle, this));
  }

 private:
  friend class base::RefCounted<AdbQueryCommand>;
  virtual ~AdbQueryCommand() {}

  void Handle(int result, const std::string& response) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbQueryCommand::Respond, this, result, response));
  }

  void Respond(int result, const std::string& response) {
    callback_.Run(result, response);
  }

  std::string query_;
  Callback callback_;
};

class AdbPagesCommand : public base::RefCounted<AdbPagesCommand> {
 public:
  explicit AdbPagesCommand(DevToolsAdbBridge* bridge,
                           const PagesCallback& callback)
     : bridge_(bridge),
       callback_(callback) {
    pages_.reset(new DevToolsAdbBridge::RemotePages());
  }

  void Run() {
    bridge_->EnumerateDevices(base::Bind(&AdbPagesCommand::ReceivedDevices,
                                         this));
  }

 private:
  friend class base::RefCounted<AdbPagesCommand>;
  virtual ~AdbPagesCommand() {}

  void ReceivedDevices(const AndroidDevices& devices) {
    devices_ = devices;
    ProcessSerials();
  }

  void ProcessSerials() {
    if (devices_.size() == 0) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&AdbPagesCommand::Respond, this));
      return;
    }

#if defined(DEBUG_DEVTOOLS)
    // For desktop remote debugging.
    if (devices_.back()->serial().empty()) {
      scoped_refptr<DevToolsAdbBridge::AndroidDevice> device =
          devices_.back();
      sockets_.push_back(std::string());
      device->set_model(kUnknownModel);
      device->HttpQuery(
          std::string(), kVersionRequest,
          base::Bind(&AdbPagesCommand::ReceivedVersion, this));
      return;
    }
#endif  // defined(DEBUG_DEVTOOLS)

    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    device->RunCommand(kDeviceModelCommand,
                       base::Bind(&AdbPagesCommand::ReceivedModel, this));
  }

  void ReceivedModel(int result, const std::string& response) {
    if (result < 0) {
      devices_.pop_back();
      ProcessSerials();
      return;
    }
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    device->set_model(response);
    device->RunCommand(kOpenedUnixSocketsCommand,
                       base::Bind(&AdbPagesCommand::ReceivedSockets, this));
  }

  void ReceivedSockets(int result,
                       const std::string& response) {
    if (result < 0) {
      devices_.pop_back();
      ProcessSerials();
      return;
    }

    ParseSocketsList(response);
    if (sockets_.size() == 0) {
      devices_.pop_back();
      ProcessSerials();
    } else {
      ProcessSockets();
    }
  }

  void ProcessSockets() {
    if (sockets_.size() == 0) {
      devices_.pop_back();
      ProcessSerials();
    } else {
      scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
      device->HttpQuery(sockets_.back(), kVersionRequest,
                        base::Bind(&AdbPagesCommand::ReceivedVersion, this));
    }
  }

  void ReceivedVersion(int result,
                       const std::string& response) {
    if (result < 0) {
      sockets_.pop_back();
      ProcessSockets();
      return;
    }

    // Parse version, append to package name if available,
    std::string body = response.substr(result);
    scoped_ptr<base::Value> value(base::JSONReader::Read(body));
    base::DictionaryValue* dict;
    if (value && value->GetAsDictionary(&dict)) {
      std::string browser;
      if (dict->GetString("Browser", &browser)) {
        socket_to_package_[sockets_.back()] = base::StringPrintf(
            "%s (%s)", socket_to_package_[sockets_.back()].c_str(),
            browser.c_str());
      }
    }

    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    device->HttpQuery(sockets_.back(), kPageListRequest,
                      base::Bind(&AdbPagesCommand::ReceivedPages, this));
  }

  void ReceivedPages(int result,
                     const std::string& response) {
    std::string socket = sockets_.back();
    sockets_.pop_back();
    if (result < 0) {
      ProcessSockets();
      return;
    }

    std::string body = response.substr(result);
    scoped_ptr<base::Value> value(base::JSONReader::Read(body));
    base::ListValue* list_value;
    if (!value || !value->GetAsList(&list_value)) {
      ProcessSockets();
      return;
    }

    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    base::Value* item;
    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      list_value->Get(i, &item);
      base::DictionaryValue* dict;
      if (!item || !item->GetAsDictionary(&dict))
        continue;
      pages_->push_back(
          new DevToolsAdbBridge::RemotePage(
              device->serial(), device->model(), socket_to_package_[socket],
              socket, *dict));
    }
    ProcessSockets();
  }

  void Respond() {
    callback_.Run(net::OK, pages_.release());
  }

  void ParseSocketsList(const std::string& response) {
    // On Android, '/proc/net/unix' looks like this:
    //
    // Num       RefCount Protocol Flags    Type St Inode Path
    // 00000000: 00000002 00000000 00010000 0001 01 331813 /dev/socket/zygote
    // 00000000: 00000002 00000000 00010000 0001 01 358606 @xxx_devtools_remote
    // 00000000: 00000002 00000000 00010000 0001 01 347300 @yyy_devtools_remote
    //
    // We need to find records with paths starting from '@' (abstract socket)
    // and containing "devtools_remote". We have to extract the inode number
    // in order to find the owning process name.

    socket_to_package_.clear();
    std::vector<std::string> entries;
    Tokenize(response, "\n", &entries);
    const std::string channel_pattern =
        base::StringPrintf(kDevToolsChannelNameFormat, "");
    for (size_t i = 1; i < entries.size(); ++i) {
      std::vector<std::string> fields;
      Tokenize(entries[i], " ", &fields);
      if (fields.size() < 8)
        continue;
      if (fields[3] != "00010000" || fields[5] != "01")
        continue;
      std::string path_field = fields[7];
      if (path_field.size() < 1 || path_field[0] != '@')
        continue;
      size_t socket_name_pos = path_field.find(channel_pattern);
      if (socket_name_pos == std::string::npos)
        continue;
      std::string socket = path_field.substr(1, path_field.size() - 2);
      sockets_.push_back(socket);
      std::string package = path_field.substr(1, socket_name_pos - 1);
      if (socket_name_pos + channel_pattern.size() < path_field.size() - 1) {
        package += path_field.substr(
            socket_name_pos + channel_pattern.size(), path_field.size() - 1);
      }
      package[0] = base::ToUpperASCII(package[0]);
      socket_to_package_[socket] = package;
    }
  }

  scoped_refptr<DevToolsAdbBridge> bridge_;
  PagesCallback callback_;
  AndroidDevices devices_;
  std::vector<std::string> sockets_;
  std::map<std::string, std::string> socket_to_package_;
  scoped_ptr<DevToolsAdbBridge::RemotePages> pages_;
};

}  // namespace

const char kDevToolsChannelNameFormat[] = "%s_devtools_remote";

class AgentHostDelegate;

typedef std::map<std::string, AgentHostDelegate*> AgentHostDelegates;

base::LazyInstance<AgentHostDelegates>::Leaky g_host_delegates =
    LAZY_INSTANCE_INITIALIZER;

DevToolsAdbBridge::Wrapper::Wrapper(Profile* profile)
    : bridge_(new DevToolsAdbBridge(profile)) {
}

DevToolsAdbBridge::Wrapper::~Wrapper() {
}

DevToolsAdbBridge* DevToolsAdbBridge::Wrapper::Get() {
  return bridge_.get();
}

// static
DevToolsAdbBridge::Factory* DevToolsAdbBridge::Factory::GetInstance() {
  return Singleton<DevToolsAdbBridge::Factory>::get();
}

// static
DevToolsAdbBridge* DevToolsAdbBridge::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<DevToolsAdbBridge::Wrapper*>(
      GetInstance()->GetServiceForBrowserContext(profile, true))->Get();
}

DevToolsAdbBridge::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "DevToolsAdbBridge",
          BrowserContextDependencyManager::GetInstance()) {}

DevToolsAdbBridge::Factory::~Factory() {}

BrowserContextKeyedService*
DevToolsAdbBridge::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DevToolsAdbBridge::Wrapper(Profile::FromBrowserContext(context));
}

DevToolsAdbBridge::AndroidDevice::AndroidDevice(const std::string& serial)
    : serial_(serial) {
}

void DevToolsAdbBridge::AndroidDevice::HttpQuery(
    const std::string& la_name,
    const std::string& request,
    const CommandCallback& callback) {
  OpenSocket(la_name, base::Bind(&AndroidDevice::OnHttpSocketOpened, this,
                                 request, callback));
}

void DevToolsAdbBridge::AndroidDevice::HttpQuery(
    const std::string& la_name,
    const std::string& request,
    const SocketCallback& callback) {
  OpenSocket(la_name, base::Bind(&AndroidDevice::OnHttpSocketOpened2, this,
                                 request, callback));
}

DevToolsAdbBridge::AndroidDevice::~AndroidDevice() {
}

void DevToolsAdbBridge::AndroidDevice::OnHttpSocketOpened(
    const std::string& request,
    const CommandCallback& callback,
    int result,
    net::StreamSocket* socket) {
  if (result != net::OK) {
    callback.Run(result, std::string());
    return;
  }
  AdbClientSocket::HttpQuery(socket, request, callback);
}

void DevToolsAdbBridge::AndroidDevice::OnHttpSocketOpened2(
    const std::string& request,
    const SocketCallback& callback,
    int result,
    net::StreamSocket* socket) {
  if (result != net::OK) {
    callback.Run(result, NULL);
    return;
  }
  AdbClientSocket::HttpQuery(socket, request, callback);
}

class AgentHostDelegate : public base::RefCountedThreadSafe<AgentHostDelegate>,
                          public content::DevToolsExternalAgentProxyDelegate {
 public:
  AgentHostDelegate(
      const std::string& id,
      const std::string& serial,
      scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
      net::StreamSocket* socket)
      : id_(id),
        serial_(serial),
        adb_thread_(adb_thread),
        socket_(socket),
        tethering_adb_filter_(kAdbPort, serial) {
    AddRef();  // Balanced in SelfDestruct.
    proxy_.reset(content::DevToolsExternalAgentProxy::Create(this));
    g_host_delegates.Get()[id] = this;
  }

  scoped_refptr<content::DevToolsAgentHost> GetAgentHost() {
    return proxy_->GetAgentHost();
  }

 private:
  friend class base::RefCountedThreadSafe<AgentHostDelegate>;

  virtual ~AgentHostDelegate() {
    g_host_delegates.Get().erase(id_);
  }

  virtual void Attach() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    adb_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AgentHostDelegate::StartListeningOnHandlerThread, this));
  }

  virtual void Detach() OVERRIDE {
    adb_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AgentHostDelegate::CloseConnection, this, net::OK, false));
  }

  virtual void SendMessageToBackend(const std::string& message) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    adb_thread_->message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&AgentHostDelegate::SendFrameOnHandlerThread, this,
                   message));
  }

  void StartListeningOnHandlerThread() {
    scoped_refptr<net::IOBuffer> response_buffer =
        new net::IOBuffer(kBufferSize);
    int result = socket_->Read(
        response_buffer.get(),
        kBufferSize,
        base::Bind(&AgentHostDelegate::OnBytesRead, this, response_buffer));
    if (result != net::ERR_IO_PENDING)
      OnBytesRead(response_buffer, result);
  }

  void OnBytesRead(scoped_refptr<net::IOBuffer> response_buffer, int result) {
    if (!socket_)
      return;

    if (result <= 0) {
      CloseConnection(net::ERR_CONNECTION_CLOSED, true);
      return;
    }

    std::string data = std::string(response_buffer->data(), result);
    response_buffer_ += data;

    int bytes_consumed;
    std::string output;
    WebSocket::ParseResult parse_result = WebSocket::DecodeFrameHybi17(
        response_buffer_, false, &bytes_consumed, &output);

    while (parse_result == WebSocket::FRAME_OK) {
      response_buffer_ = response_buffer_.substr(bytes_consumed);
      if (!tethering_adb_filter_.ProcessIncomingMessage(output)) {
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
            base::Bind(&AgentHostDelegate::OnFrameRead, this, output));
      }
      parse_result = WebSocket::DecodeFrameHybi17(
          response_buffer_, false, &bytes_consumed, &output);
    }

    if (parse_result == WebSocket::FRAME_ERROR ||
        parse_result == WebSocket::FRAME_CLOSE) {
      CloseConnection(net::ERR_CONNECTION_CLOSED, true);
      return;
    }

    result = socket_->Read(
        response_buffer.get(),
        kBufferSize,
        base::Bind(&AgentHostDelegate::OnBytesRead, this, response_buffer));
    if (result != net::ERR_IO_PENDING)
      OnBytesRead(response_buffer, result);
  }

  void SendFrameOnHandlerThread(const std::string& data) {
    tethering_adb_filter_.ProcessOutgoingMessage(data);
    int mask = base::RandInt(0, 0x7FFFFFFF);
    std::string encoded_frame = WebSocket::EncodeFrameHybi17(data, mask);
    request_buffer_ += encoded_frame;
    if (request_buffer_.length() == encoded_frame.length())
      SendPendingRequests(0);
  }

  void SendPendingRequests(int result) {
    if (!socket_)
      return;
    if (result < 0) {
      CloseConnection(result, true);
      return;
    }
    request_buffer_ = request_buffer_.substr(result);
    if (request_buffer_.empty())
      return;

    scoped_refptr<net::StringIOBuffer> buffer =
        new net::StringIOBuffer(request_buffer_);
    result = socket_->Write(buffer.get(), buffer->size(),
                            base::Bind(&AgentHostDelegate::SendPendingRequests,
                                       this));
    if (result != net::ERR_IO_PENDING)
      SendPendingRequests(result);
  }

  void CloseConnection(int result, bool initiated_by_me) {
    if (!socket_)
      return;
    socket_->Disconnect();
    socket_.reset();
    if (initiated_by_me) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          base::Bind(&AgentHostDelegate::OnSocketClosed, this, result));
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AgentHostDelegate::SelfDestruct, this));
  }

  void SelfDestruct() {
    Release();  // Balanced in constructor.
  }

  void OnFrameRead(const std::string& message) {
    proxy_->DispatchOnClientHost(message);
  }

  void OnSocketClosed(int result) {
    proxy_->ConnectionClosed();
  }

  std::string id_;
  std::string serial_;
  scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread_;
  scoped_ptr<net::StreamSocket> socket_;
  scoped_ptr<content::DevToolsExternalAgentProxy> proxy_;
  std::string response_buffer_;
  std::string request_buffer_;
  TetheringAdbFilter tethering_adb_filter_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};

class AdbAttachCommand : public base::RefCounted<AdbAttachCommand> {
 public:
  AdbAttachCommand(DevToolsAdbBridge* bridge,
                   const std::string& serial,
                   const std::string& socket,
                   const std::string& debug_url,
                   const std::string& frontend_url)
      : bridge_(bridge),
        serial_(serial),
        socket_(socket),
        debug_url_(debug_url),
        frontend_url_(frontend_url) {
  }

  void Run() {
    bridge_->EnumerateDevices(base::Bind(&AdbAttachCommand::ReceivedDevices,
                                         this));
  }

 private:
  friend class base::RefCounted<AdbAttachCommand>;
  virtual ~AdbAttachCommand() {}

  void ReceivedDevices(const AndroidDevices& devices) {
    for (AndroidDevices::const_iterator it = devices.begin();
         it != devices.end(); ++it) {
      if ((*it)->serial() == serial_) {
        (*it)->HttpQuery(
            socket_,
            base::StringPrintf(kWebSocketUpgradeRequest, debug_url_.c_str()),
            base::Bind(&AdbAttachCommand::Handle, this));
        break;
      }
    }
  }

  void Handle(int result, net::StreamSocket* socket) {
    if (result != net::OK || socket == NULL)
      return;

    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbAttachCommand::OpenDevToolsWindow, this, socket));
  }

  void OpenDevToolsWindow(net::StreamSocket* socket) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    std::string id = base::StringPrintf("%s:%s", serial_.c_str(),
                                        debug_url_.c_str());
    AgentHostDelegates::iterator it = g_host_delegates.Get().find(id);
    AgentHostDelegate* delegate;
    if (it != g_host_delegates.Get().end())
      delegate = it->second;
    else
      delegate = new AgentHostDelegate(id, serial_, bridge_->adb_thread_,
                                       socket);
    DevToolsWindow::OpenExternalFrontend(
        bridge_->profile_, frontend_url_, delegate->GetAgentHost().get());
  }

  scoped_refptr<DevToolsAdbBridge> bridge_;
  std::string serial_;
  std::string socket_;
  std::string debug_url_;
  std::string frontend_url_;
};

DevToolsAdbBridge::RemotePage::RemotePage(const std::string& serial,
                                          const std::string& model,
                                          const std::string& package,
                                          const std::string& socket,
                                          const base::DictionaryValue& value)
    : serial_(serial),
      model_(model),
      package_(package),
      socket_(socket) {
  value.GetString("id", &id_);
  value.GetString("url", &url_);
  value.GetString("title", &title_);
  value.GetString("descirption", &description_);
  value.GetString("faviconUrl", &favicon_url_);
  value.GetString("webSocketDebuggerUrl", &debug_url_);
  value.GetString("devtoolsFrontendUrl", &frontend_url_);

  if (debug_url_.find("ws://") == 0)
    debug_url_ = debug_url_.substr(5);
  else
    debug_url_ = "";

  size_t ws_param = frontend_url_.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url_ = frontend_url_.substr(0, ws_param);
  if (frontend_url_.find("http:") == 0)
    frontend_url_ = "https:" + frontend_url_.substr(5);
}

DevToolsAdbBridge::RemotePage::~RemotePage() {
}

DevToolsAdbBridge::RefCountedAdbThread*
DevToolsAdbBridge::RefCountedAdbThread::instance_ = NULL;

// static
scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread>
DevToolsAdbBridge::RefCountedAdbThread::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!instance_)
    new RefCountedAdbThread();
  return instance_;
}

DevToolsAdbBridge::RefCountedAdbThread::RefCountedAdbThread() {
  instance_ = this;
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    delete thread_;
    thread_ = NULL;
  }
}

base::MessageLoop* DevToolsAdbBridge::RefCountedAdbThread::message_loop() {
  return thread_ ? thread_->message_loop() : NULL;
}

// static
void DevToolsAdbBridge::RefCountedAdbThread::StopThread(base::Thread* thread) {
  thread->Stop();
}

DevToolsAdbBridge::RefCountedAdbThread::~RefCountedAdbThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  instance_ = NULL;
  if (!thread_)
    return;
  // Shut down thread on FILE thread to join into IO.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RefCountedAdbThread::StopThread, thread_));
}

DevToolsAdbBridge::DevToolsAdbBridge(Profile* profile)
    : profile_(profile),
      adb_thread_(RefCountedAdbThread::GetInstance()),
      has_message_loop_(adb_thread_->message_loop() != NULL) {
  rsa_key_.reset(AndroidRSAPrivateKey(profile));
}

void DevToolsAdbBridge::EnumerateDevices(
    const AndroidDevicesCallback& callback) {
  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::Bind(&DevToolsAdbBridge::ReceivedDevices, this, callback));
}

void DevToolsAdbBridge::Query(
    const std::string query,
    const Callback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_) {
    callback.Run(net::ERR_FAILED, "Could not start ADB thread");
    return;
  }
  scoped_refptr<AdbQueryCommand> command(new AdbQueryCommand(query, callback));
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AdbQueryCommand::Run, command));
}

void DevToolsAdbBridge::Pages(const PagesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  scoped_refptr<AdbPagesCommand> command(
      new AdbPagesCommand(this, callback));
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AdbPagesCommand::Run, command));
}

void DevToolsAdbBridge::Attach(const std::string& serial,
                               const std::string& socket,
                               const std::string& debug_url,
                               const std::string& frontend_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  scoped_refptr<AdbAttachCommand> command(
      new AdbAttachCommand(this, serial, socket,
                           debug_url, frontend_url));
  adb_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&AdbAttachCommand::Run, command));
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void DevToolsAdbBridge::ReceivedDevices(const AndroidDevicesCallback& callback,
                                        int result,
                                        const std::string& response) {
  AndroidDevices devices;
#if defined(DEBUG_DEVTOOLS)
  devices.push_back(new AdbDeviceImpl(""));  // For desktop remote debugging.
#endif  // defined(DEBUG_DEVTOOLS)

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRemoteDebuggingRawUSB)) {
    AndroidUsbDevices usb_devices;
    AndroidUsbDevice::Enumerate(profile_, rsa_key_.get(), &usb_devices);
    for (AndroidUsbDevices::iterator it = usb_devices.begin();
         it != usb_devices.end(); ++it) {
      devices.push_back(new UsbDeviceImpl(*it));
    }
  }

  if (result != net::OK) {
    callback.Run(devices);
    return;
  }

  std::vector<std::string> serials;
  Tokenize(response, "\n", &serials);
  for (size_t i = 0; i < serials.size(); ++i) {
    std::vector<std::string> tokens;
    Tokenize(serials[i], "\t ", &tokens);
    devices.push_back(new AdbDeviceImpl(tokens[0]));
  }
  callback.Run(devices);
}