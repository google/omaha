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

#ifndef OMAHA_CLIENT_CLIENT_UTILS_H_
#define OMAHA_CLIENT_CLIENT_UTILS_H_

#include <windows.h>
#include <atlstr.h>

namespace omaha {

namespace client_utils {

// Displays the error message in a new UI instance.
// Returns whether a UI was successfully displayed. Resources must be loaded.
bool DisplayError(bool is_machine,
                  const CString& bundle_name,
                  HRESULT error,
                  int extra_code1,
                  const CString& error_text,
                  const CString& app_id,
                  const CString& language_id,
                  const GUID& iid,
                  const CString& brand_code);

// Displays a dialog prompting the user to choose to continue with installing
// the bundle on a per-user basis. should_continue is set to true if the user
// chooses to proceed.
// Returns whether a UI was successfully displayed. Resources must be loaded.
bool DisplayContinueAsNonAdmin(const CString& bundle_name,
                               bool* should_continue);

CString GetDefaultApplicationName();

CString GetDefaultBundleName();

CString GetUpdateAllAppsBundleName();

CString GetInstallerDisplayName(const CString& bundle_name);

}  // namespace client_utils

}  // namespace omaha

#endif  // OMAHA_CLIENT_CLIENT_UTILS_H_
