// Copyright 2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================

#include "omaha/mi_exe_stub/x86_encoder/bcj2_encoder.h"
#include <string>
#include <vector>
#include "omaha/base/app_util.h"
#include "omaha/base/utils.h"
#include "omaha/testing/unit_test.h"
extern "C" {
#include "third_party/lzma/files/C/Bcj2.h"
}

namespace omaha {

TEST(Bcj2EncoderTest, EmptyBuffer) {
  std::string input;
  std::string output1;
  std::string output2;
  std::string output3;
  std::string output4;
  ASSERT_TRUE(Bcj2Encode(input, &output1, &output2, &output3, &output4));
  EXPECT_EQ("", output1);
  EXPECT_EQ("", output2);
  EXPECT_EQ("", output3);
  EXPECT_EQ(std::string(5, '\0'), output4);
}

// Test that the transform is reversible.
TEST(Bcj2EncoderTest, Reversible) {
  // The victim program is the unit test itself.
  CString module_path = app_util::GetModulePath(NULL);
  ASSERT_FALSE(module_path.IsEmpty());

  std::vector<byte> raw_file;
  ASSERT_HRESULT_SUCCEEDED(
      ReadEntireFileShareMode(module_path, 0, FILE_SHARE_READ, &raw_file));
  const std::string input(reinterpret_cast<char*>(&raw_file[0]),
                          raw_file.size());
  std::string output1;
  std::string output2;
  std::string output3;
  std::string output4;
  ASSERT_TRUE(Bcj2Encode(input, &output1, &output2, &output3, &output4));

  std::string decoded_output;
  decoded_output.resize(raw_file.size());
  ASSERT_EQ(SZ_OK, Bcj2_Decode(reinterpret_cast<const uint8*>(output1.data()),
                               output1.size(),
                               reinterpret_cast<const uint8*>(output2.data()),
                               output2.size(),
                               reinterpret_cast<const uint8*>(output3.data()),
                               output2.size(),
                               reinterpret_cast<const uint8*>(output4.data()),
                               output4.size(),
                               reinterpret_cast<uint8*>(&decoded_output[0]),
                               decoded_output.size()));
  EXPECT_EQ(input, decoded_output);
}

}  // namespace omaha
