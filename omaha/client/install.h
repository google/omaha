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

// Functions related to the /install process.

#ifndef OMAHA_CLIENT_INSTALL_H_
#define OMAHA_CLIENT_INSTALL_H_

#include <windows.h>
#include <atlstr.h>
#include "omaha/common/const_goopdate.h"

namespace omaha {

struct CommandLineArgs;

// Elevates if necessary and then installs.
HRESULT Install(bool is_interactive,
                bool is_app_install,
                bool is_eula_required,
                bool is_oem_install,
                bool is_enterprise_install,
                bool is_install_elevated_instance,
                const CString& install_cmd_line,
                const CommandLineArgs& args,
                bool* is_machine,
                bool* has_ui_been_displayed);

// Installs Omaha and/or apps in OEM state.
HRESULT OemInstall(bool is_interactive,
                   bool is_app_install,
                   bool is_eula_required,
                   bool is_install_elevated_instance,
                   const CString& install_cmd_line,
                   const CommandLineArgs& args,
                   bool* is_machine,
                   bool* has_ui_been_displayed);

}  // namespace omaha

#endif  // OMAHA_CLIENT_INSTALL_H_
