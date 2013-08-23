// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/nacl_process_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
TEST(NaClProcessHostTest, AddressSpaceAllocation) {
  size_t size = 1 << 20;  // 1 MB
  void* addr = nacl::AllocateAddressSpaceASLR(GetCurrentProcess(), size);
  bool success = VirtualFree(addr, 0, MEM_RELEASE);
  ASSERT_TRUE(success);
}
#endif
