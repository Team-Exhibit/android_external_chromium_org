// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <string>

#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "media/audio/cras/audio_manager_cras.h"
#include "media/audio/cras/cras_input.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AtLeast;
using testing::Ge;
using testing::InvokeWithoutArgs;
using testing::StrictMock;

namespace media {

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD5(OnData, void(
      AudioInputStream*, const uint8*, uint32, uint32, double));
  MOCK_METHOD1(OnError, void(AudioInputStream*));
  MOCK_METHOD1(OnClose, void(AudioInputStream*));
};

class MockAudioManagerCrasInput : public AudioManagerCras {
 public:
  // We need to override this function in order to skip checking the number
  // of active output streams. It is because the number of active streams
  // is managed inside MakeAudioInputStream, and we don't use
  // MakeAudioInputStream to create the stream in the tests.
  virtual void ReleaseInputStream(AudioInputStream* stream) OVERRIDE {
    DCHECK(stream);
    delete stream;
  }
};

class CrasInputStreamTest : public testing::Test {
 protected:
  CrasInputStreamTest() {
    mock_manager_.reset(new StrictMock<MockAudioManagerCrasInput>());
  }

  virtual ~CrasInputStreamTest() {
  }

  CrasInputStream* CreateStream(ChannelLayout layout) {
    return CreateStream(layout, kTestFramesPerPacket);
  }

  CrasInputStream* CreateStream(ChannelLayout layout,
                                int32 samples_per_packet) {
    AudioParameters params(kTestFormat,
                           layout,
                           kTestSampleRate,
                           kTestBitsPerSample,
                           samples_per_packet);
    return new CrasInputStream(params, mock_manager_.get(),
                               AudioManagerBase::kDefaultDeviceId);
  }

  void CaptureSomeFrames(const AudioParameters &params,
                         unsigned int duration_ms) {
    CrasInputStream* test_stream = new CrasInputStream(
        params, mock_manager_.get(), AudioManagerBase::kDefaultDeviceId);

    ASSERT_TRUE(test_stream->Open());

    // Allow 8 frames variance for SRC in the callback.  Different numbers of
    // samples can be provided when doing non-integer SRC.  For example
    // converting from 192k to 44.1k is a ratio of 4.35 to 1.
    MockAudioInputCallback mock_callback;
    unsigned int expected_size = (kTestFramesPerPacket - 8) *
                                 params.channels() *
                                 params.bits_per_sample() / 8;

    base::WaitableEvent event(false, false);

    EXPECT_CALL(mock_callback,
                OnData(test_stream, _, Ge(expected_size), _, _))
        .WillOnce(InvokeWithoutArgs(&event, &base::WaitableEvent::Signal));

    test_stream->Start(&mock_callback);

    // Wait for samples to be captured.
    EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));

    test_stream->Stop();

    EXPECT_CALL(mock_callback, OnClose(test_stream)).Times(1);
    test_stream->Close();
  }

  static const unsigned int kTestBitsPerSample;
  static const unsigned int kTestCaptureDurationMs;
  static const ChannelLayout kTestChannelLayout;
  static const AudioParameters::Format kTestFormat;
  static const uint32 kTestFramesPerPacket;
  static const int kTestSampleRate;

  scoped_ptr<StrictMock<MockAudioManagerCrasInput> > mock_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrasInputStreamTest);
};

const unsigned int CrasInputStreamTest::kTestBitsPerSample = 16;
const unsigned int CrasInputStreamTest::kTestCaptureDurationMs = 250;
const ChannelLayout CrasInputStreamTest::kTestChannelLayout =
    CHANNEL_LAYOUT_STEREO;
const AudioParameters::Format CrasInputStreamTest::kTestFormat =
    AudioParameters::AUDIO_PCM_LINEAR;
const uint32 CrasInputStreamTest::kTestFramesPerPacket = 1000;
const int CrasInputStreamTest::kTestSampleRate = 44100;

TEST_F(CrasInputStreamTest, OpenMono) {
  CrasInputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, OpenStereo) {
  CrasInputStream* test_stream = CreateStream(CHANNEL_LAYOUT_STEREO);
  EXPECT_TRUE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, BadBitsPerSample) {
  AudioParameters bad_bps_params(kTestFormat,
                                 kTestChannelLayout,
                                 kTestSampleRate,
                                 kTestBitsPerSample - 1,
                                 kTestFramesPerPacket);
  CrasInputStream* test_stream = new CrasInputStream(
      bad_bps_params, mock_manager_.get(), AudioManagerBase::kDefaultDeviceId);
  EXPECT_FALSE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, BadFormat) {
  AudioParameters bad_format_params(AudioParameters::AUDIO_LAST_FORMAT,
                                    kTestChannelLayout,
                                    kTestSampleRate,
                                    kTestBitsPerSample,
                                    kTestFramesPerPacket);
  CrasInputStream* test_stream = new CrasInputStream(
      bad_format_params, mock_manager_.get(),
      AudioManagerBase::kDefaultDeviceId);
  EXPECT_FALSE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, BadSampleRate) {
  AudioParameters bad_rate_params(kTestFormat,
                                  kTestChannelLayout,
                                  0,
                                  kTestBitsPerSample,
                                  kTestFramesPerPacket);
  CrasInputStream* test_stream = new CrasInputStream(
      bad_rate_params, mock_manager_.get(), AudioManagerBase::kDefaultDeviceId);
  EXPECT_FALSE(test_stream->Open());
  test_stream->Close();
}

TEST_F(CrasInputStreamTest, SetGetVolume) {
  CrasInputStream* test_stream = CreateStream(CHANNEL_LAYOUT_MONO);
  EXPECT_TRUE(test_stream->Open());

  double max_volume = test_stream->GetMaxVolume();
  EXPECT_GE(max_volume, 1.0);

  test_stream->SetVolume(max_volume / 2);

  double new_volume = test_stream->GetVolume();

  EXPECT_GE(new_volume, 0.0);
  EXPECT_LE(new_volume, max_volume);

  test_stream->Close();
}

TEST_F(CrasInputStreamTest, CaptureFrames) {
  const unsigned int rates[] =
      {8000, 16000, 22050, 32000, 44100, 48000, 96000, 192000};

  for (unsigned int i = 0; i < ARRAY_SIZE(rates); i++) {
    SCOPED_TRACE(testing::Message() << "Mono " << rates[i] << "Hz");
    AudioParameters params_mono(kTestFormat,
                                CHANNEL_LAYOUT_MONO,
                                rates[i],
                                kTestBitsPerSample,
                                kTestFramesPerPacket);
    CaptureSomeFrames(params_mono, kTestCaptureDurationMs);
  }

  for (unsigned int i = 0; i < ARRAY_SIZE(rates); i++) {
    SCOPED_TRACE(testing::Message() << "Stereo " << rates[i] << "Hz");
    AudioParameters params_stereo(kTestFormat,
                                  CHANNEL_LAYOUT_STEREO,
                                  rates[i],
                                  kTestBitsPerSample,
                                  kTestFramesPerPacket);
    CaptureSomeFrames(params_stereo, kTestCaptureDurationMs);
  }
}

}  // namespace media
