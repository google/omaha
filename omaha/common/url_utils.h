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

#include <atlstr.h>
#include <utility>
#include <vector>

namespace omaha {

using QueryElement = std::pair<CString, CString>;

// Assembles the parameters in |query_params| into |query|, escaping the values
// as appropriate. Returns S_OK on success, or a failure HRESULT otherwise.
// |query| is not modified in case of error.
HRESULT BuildQueryString(const std::vector<QueryElement>& query_params,
                         CString* query);

}  // namespace omaha
