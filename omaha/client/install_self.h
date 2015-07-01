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

// Functions related to the installing Omaha.

#ifndef OMAHA_CLIENT_INSTALL_SELF_H_
#define OMAHA_CLIENT_INSTALL_SELF_H_

#include <windows.h>
#include <atlstr.h>
#include "omaha/common/ping_event.h"

namespace omaha {

struct CommandLineExtraArgs;

namespace install_self {

// Marks Omaha EULA as accepted by deleting the registry value.
// Does not touch apps' EULA state.
HRESULT SetEulaAccepted(bool is_machine);

// Installs Omaha in the /install case. Does not install any applications.
HRESULT InstallSelf(bool is_machine,
                    bool is_eula_required,
                    bool is_oem_install,
                    bool is_enterprise_install,
                    const CString& current_version,
                    const CString& install_source,
                    const CommandLineExtraArgs& extra_args,
                    const CString& session_id,
                    int* extra_code1);

// Updates Omaha.
HRESULT UpdateSelf(bool is_machine, const CString& session_id);

// Repairs Omaha. Used for Code Red recovery.
HRESULT Repair(bool is_machine);

// Verifies that Omaha is either properly installed or uninstalled completely.
void CheckInstallStateConsistency(bool is_machine);

// Uninstalls all Omaha versions if Omaha can be uninstalled.
HRESULT UninstallSelf(bool is_machine, bool send_uninstall_ping);

// Reads the error info for silent updates from the registry if present and
// deletes it. Returns true if the data is valid.
bool ReadAndClearUpdateErrorInfo(bool is_machine,
                                 DWORD* error_code,
                                 DWORD* extra_code1,
                                 CString* version);

}  // namespace install_self

}  // namespace omaha

#endif  // OMAHA_CLIENT_INSTALL_SELF_H_
