// Copyright 2019 Google LLC.
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

#include "omaha/common/url_utils.h"

#include <atlstr.h>
#include <utility>
#include <vector>

#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(BuildQueryStringTest, Do) {
  std::vector<QueryElement> params;
  params.push_back(std::make_pair(L"one", L"1"));
  params.push_back(std::make_pair(L"2", L"two"));
  CString query;
  EXPECT_HRESULT_SUCCEEDED(BuildQueryString(params, &query));
  EXPECT_STREQ(query, L"one=1&2=two");
}

TEST(BuildQueryStringTest, EncodesQueryElements) {
  std::vector<QueryElement> params;
  params.push_back(std::make_pair(L"first", L" '`"));
  params.push_back(std::make_pair(L"second", L"&="));
  CString query;
  EXPECT_HRESULT_SUCCEEDED(BuildQueryString(params, &query));
  EXPECT_STREQ(query, L"first=%20'%60&second=%26=");
}

}  // namespace omaha
