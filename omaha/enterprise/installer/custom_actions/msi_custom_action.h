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

#ifndef OMAHA_ENTERPRISE_INSTALLER_CUSTOM_ACTIONS_MSI_CUSTOM_ACTION_H_
#define OMAHA_ENTERPRISE_INSTALLER_CUSTOM_ACTIONS_MSI_CUSTOM_ACTION_H_

#include <string.h>

namespace custom_action {

// Gets the value of the property named |property_name|, putting it in
// |property_value|. Returns true if a (possibly empty) value is read,
// or false on error.
bool GetProperty(MSIHANDLE install,
                 const wchar_t* property_name,
                 std::wstring* property_value);

}  // namespace custom_action

#endif  // OMAHA_ENTERPRISE_INSTALLER_CUSTOM_ACTIONS_MSI_CUSTOM_ACTION_H_
