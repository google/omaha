// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include "omaha/testing/unit_test.h"

namespace omaha {

// There is a very small probability these tests could fail.

TEST(RandUtilTest, RandBytes) {
  uint32 random_uint = 0;
  EXPECT_TRUE(RandBytes(&random_uint, sizeof(random_uint)));
  EXPECT_NE(random_uint, 0);

  uint32 another_random_uint = 0;
  EXPECT_TRUE(RandBytes(&another_random_uint, sizeof(another_random_uint)));
  EXPECT_NE(another_random_uint, 0);

  EXPECT_NE(random_uint, another_random_uint);
}

TEST(RandUtilTest, RandUint32) {
  uint32 random_uint = 0;
  EXPECT_TRUE(RandUint32(&random_uint));
  EXPECT_NE(random_uint, 0);

  uint32 another_random_uint = 0;
  EXPECT_TRUE(RandUint32(&another_random_uint));
  EXPECT_NE(another_random_uint, 0);

  EXPECT_NE(random_uint, another_random_uint);
}

}  // namespace omaha

