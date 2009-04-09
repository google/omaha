// Copyright 2007-2009 Google Inc.
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

#include <windows.h>
#include <winhttp.h>
#include "omaha/common/app_util.h"
#include "omaha/net/bits_request.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Moves the job to error state if no progress at all is made for 10 seconds.
TEST(BitsRequestTest, Send) {
  BitsRequest bits_request;
  bits_request.set_no_progress_timeout(10);   // 10 seconds.
  CString temp_dir = app_util::GetTempDir();
  CString temp_file;
  EXPECT_TRUE(::GetTempFileName(temp_dir, _T("tmp"), 0,
                                CStrBuf(temp_file, MAX_PATH)));
  bits_request.set_filename(temp_file);
  bits_request.set_url(_T("http://dl.google.com/update2/UpdateData.bin"));
  EXPECT_HRESULT_SUCCEEDED(bits_request.Send());
  EXPECT_EQ(HTTP_STATUS_OK, bits_request.GetHttpStatusCode());

  bits_request.set_low_priority(true);
  EXPECT_HRESULT_SUCCEEDED(bits_request.Send());
  EXPECT_EQ(HTTP_STATUS_OK, bits_request.GetHttpStatusCode());
}

}   // namespace omaha

