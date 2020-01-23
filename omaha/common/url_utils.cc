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

#include "omaha/base/debug.h"
#include "omaha/base/string.h"

namespace omaha {

HRESULT BuildQueryString(const std::vector<QueryElement>& query_params,
                         CString* query) {
  ASSERT1(query);
  CString query_part;

  CString encoded_value;
  for (auto const&[key, value] : query_params) {
    HRESULT hr = StringEscape(value, false, &encoded_value);
    if (FAILED(hr)) {
      return hr;
    }

    if (!query_part.IsEmpty()) {
      query_part.AppendChar(_T('&'));
    }
    query_part.Append(key);
    query_part.AppendChar(_T('='));
    query_part.Append(encoded_value);
  }

  *query = query_part;
  return S_OK;
}

}  // namespace omaha
