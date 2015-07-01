// Copyright 2013 Google Inc.
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
//

#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <limits>
#include <string>
#include <vector>

#include "omaha/enterprise/installer/custom_actions/msi_custom_action.h"

namespace custom_action {

// Gets the value of the property named |property_name|, putting it in
// |property_value|. Returns true if a (possibly empty) value is read,
// or false on error.
bool GetProperty(MSIHANDLE install,
                 const wchar_t* property_name,
                 std::wstring* property_value) {
  DWORD value_len = 0;
  UINT result = ERROR_SUCCESS;
  std::vector<wchar_t> buffer;
  do {
    // Make space to hold the string terminator.
    buffer.resize(++value_len);
    result = ::MsiGetProperty(install, property_name, &buffer[0], &value_len);
  } while (result == ERROR_MORE_DATA &&
           value_len <= std::numeric_limits<DWORD>::max() - 1);

  if (result == ERROR_SUCCESS) {
    property_value->resize(value_len);
    property_value->assign(buffer.begin(), buffer.end());
  } else {
    property_value->clear();
  }

  return result == ERROR_SUCCESS;
}

}  // namespace custom_action
