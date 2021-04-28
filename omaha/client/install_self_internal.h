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

#ifndef OMAHA_CLIENT_INSTALL_SELF_INTERNAL_H_
#define OMAHA_CLIENT_INSTALL_SELF_INTERNAL_H_

#include <windows.h>
#include <atlstr.h>

#include "omaha/common/const_goopdate.h"

namespace omaha {

namespace install_self {

namespace internal {

// Performs a self-update.
HRESULT DoSelfUpdate(bool is_machine, int* extra_code1);

// Does the actual work of installing Omaha for all cases.
HRESULT DoInstallSelf(bool is_machine,
                      bool is_self_update,
                      bool is_eula_required,
                      RuntimeMode runtime_mode,
                      int* extra_code1);

// Checks that the Omaha system requirements are met. Returns an error if not.
HRESULT CheckSystemRequirements();

// Returns true if it can instantiate MSXML parser.
bool HasXmlParser();

// Sets or clears the flag that prevents Google Update from using the network
// until the EULA has been accepted based on whether the eularequired flag
// appears on the command line.
HRESULT SetEulaRequiredState(bool is_machine, bool is_eula_required);

// Marks Google Update EULA as not accepted if it is not already installed.
// Does not touch apps' EULA state.
HRESULT SetEulaNotAccepted(bool is_machine);

// Sets Omaha's IID in registry if one is specified, replacing existing IID.
HRESULT SetInstallationId(const CString& omaha_client_state_key_path,
                          const GUID& iid);

// TODO(omaha3): Maybe find a different home for these two methods.
// Writes error info for silent updates to the registry so the installed Omaha
// can send an update failed ping.
void PersistUpdateErrorInfo(bool is_machine,
                            HRESULT error,
                            int extra_code1,
                            const CString& version);

}  // namespace internal

}  // namespace install_self

}  // namespace omaha

#endif  // OMAHA_CLIENT_INSTALL_SELF_INTERNAL_H_
