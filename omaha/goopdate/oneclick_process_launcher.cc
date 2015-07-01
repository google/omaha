// Copyright 2011 Google Inc.
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

#include "omaha/goopdate/oneclick_process_launcher.h"

#include "base/scoped_ptr.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/goopdate/app_command.h"
#include "omaha/goopdate/app_command_configuration.h"

namespace omaha {

OneClickProcessLauncher::OneClickProcessLauncher() {}

OneClickProcessLauncher::~OneClickProcessLauncher() {}

STDMETHODIMP OneClickProcessLauncher::LaunchAppCommand(
    const WCHAR* app_guid, const WCHAR* cmd_id) {
  ASSERT1(app_guid);
  ASSERT1(cmd_id);

  if (!app_guid || !cmd_id) {
    return E_INVALIDARG;
  }

  CORE_LOG(L3, (_T("[OneClickProcessLauncher::LaunchAppCommand]")
                _T("[app %s][cmd %s]"), app_guid, cmd_id));

  // Allocate a session ID for the ping that this call will generate.
  // TODO(omaha3): Are there any situations where this control can be
  // instantiated outside of the context of an Update3Web/OneClick
  // webpage?  If not, we should consider adding a function to
  // OneClickProcessLauncher() to modify the session ID it uses.
  CString session_id;
  GetGuid(&session_id);

  scoped_ptr<AppCommandConfiguration> configuration;
  HRESULT hr = AppCommandConfiguration::Load(app_guid,
                                             is_machine(),
                                             cmd_id,
                                             address(configuration));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to load command configuration][0x%x]"), hr));
    return hr;
  }

  if (!configuration->is_web_accessible()) {
    return E_ACCESSDENIED;
  }

  if (!is_machine()) {
    // Execute directly at medium integrity for user-level mode
    scoped_process process;
    scoped_ptr<AppCommand> app_command(configuration->Instantiate(session_id));
    return app_command->Execute(NULL, std::vector<CString>(), address(process));
  }

  // Elevate to high integrity for machine-level mode
  CComPtr<IProcessLauncher> process_launcher;
  hr = process_launcher.CoCreateInstance(__uuidof(ProcessLauncherClass));
  if (FAILED(hr)) {
    return hr;
  }

  ULONG_PTR phandle = NULL;
  DWORD process_id = ::GetCurrentProcessId();

  hr = process_launcher->LaunchCmdElevated(
      app_guid, cmd_id, process_id, &phandle);

  if (SUCCEEDED(hr)) {
    ::CloseHandle(reinterpret_cast<HANDLE>(phandle));
  }

  return hr;
}

}  // namespace omaha
