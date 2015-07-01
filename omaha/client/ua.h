// Copyright 2009-2010 Google Inc.
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

// Functions related to the /ua "update apps" process.

#ifndef OMAHA_CLIENT_UA_H_
#define OMAHA_CLIENT_UA_H_

#include <windows.h>
#include <atlstr.h>

namespace omaha {

// Returns true if a server update check is due.
bool ShouldCheckForUpdates(bool is_machine);

// Performs the duties of the silent auto-update process /ua.
HRESULT UpdateApps(bool is_machine,
                   bool is_interactive,
                   bool is_on_demand,
                   const CString& install_source,
                   const CString& display_language,
                   bool* has_ui_been_displayed);

}  // namespace omaha

#endif  // OMAHA_CLIENT_UA_H_
