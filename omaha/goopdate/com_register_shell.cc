// Copyright 2014 Google Inc.
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

#include <shlwapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <windows.h>
#include "omaha/base/app_util.h"
#include "omaha/base/command_line_parser.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/third_party/smartany/scoped_any.h"

// GoogleUpdate.exe is running in 32-bit mode and it cannot directly load
// 64-bit COM server DLL for register/unregister purpose. So create this
// application to do that.

namespace omaha {

// This function is same as goopdate_util::RedirectHKCR() except this one opens
// registry key without adding flag KEY_WOW64_32KEY.
// TODO(omaha): Revisit goodate_util::RedirectHKCR() and check whether adding
// flag KEY_WOW64_32KEY is necessary. If not, we can remove this function. See
// http://b/13084942.
HRESULT RedirectHKCR(bool is_machine) {
  scoped_hkey key;
  LONG res = ::RegOpenKeyEx(is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                            _T("Software\\Classes"),
                            0,
                            KEY_ALL_ACCESS,
                            address(key));
  if (res != ERROR_SUCCESS) {
    ASSERT(FALSE, (_T("RegOpenKeyEx [ismachine=%d][%d]"), is_machine, res));
    return HRESULT_FROM_WIN32(res);
  }

  res = ::RegOverridePredefKey(HKEY_CLASSES_ROOT, get(key));
  if (res != ERROR_SUCCESS) {
    ASSERT(false, (_T("RedirectHKCR - RegOverridePredefKey fail %d"), res));
    return HRESULT_FROM_WIN32(res);
  }

  return S_OK;
}

HRESULT RegisterOrUnregisterProxies64(bool is_machine, bool is_register) {
  // ATL by default registers the control to HKCR and we want to register
  // either in HKLM, or in HKCU, depending on whether we are laying down
  // the system googleupdate, or the user googleupdate.
  // We solve this for the user goopdate case by:
  // * Having the RGS file take a HKROOT parameter that translates to either
  //   HKLM or HKCU.
  // * Redirecting HKCR to HKCU\software\classes, for a user installation, to
  //   cover Proxy registration.
  // For the machine case, we still redirect HKCR to HKLM\\Software\\Classes,
  // to ensure that Proxy registration happens in HKLM.
  HRESULT hr = RedirectHKCR(is_machine);
  ASSERT1(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }
  // We need to stop redirecting at the end of this function.
  ON_SCOPE_EXIT(goopdate_utils::RemoveRedirectHKCR);

  CPath ps_dll(app_util::GetCurrentModuleDirectory());
  if (!ps_dll.Append(is_machine ? kPSFileNameMachine64 : kPSFileNameUser64)) {
    return HRESULTFromLastError();
  }

  ASSERT1(!is_register || ps_dll.FileExists());
  hr = is_register ? RegisterDll(ps_dll) : UnregisterDll(ps_dll);
  CORE_LOG(L3, (_T("[  PS][%s][0x%x]"), ps_dll, hr));
  return hr;
}

}  // namespace omaha

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  omaha::EnableSecureDllLoading();

  TCHAR* cmd_line = GetCommandLine();
  omaha::CommandLineParser parser;
  HRESULT hr = parser.ParseFromString(cmd_line);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("Failed to parse command line argument: [%s]"), cmd_line));
    return hr;
  }

  const bool is_machine = !parser.HasSwitch(_T("user"));
  const bool is_register = !parser.HasSwitch(_T("unregister"));
  return omaha::RegisterOrUnregisterProxies64(is_machine, is_register);
}
