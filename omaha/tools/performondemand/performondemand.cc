// Copyright 2008-2009 Google Inc.
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
// A simple tool for performing and interacting with on demand updates.
#include "omaha/tools/performondemand/performondemand.h"
#include <windows.h>
#include <sddl.h>
#include <shlobj.h>
#include <atltime.h>
#include <tchar.h>
#include "omaha/base/system.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"

namespace omaha {

bool ParseParams(int argc, TCHAR* argv[], CString* guid, bool* is_machine,
                 bool* is_update_check_only, int* timeout) {
  ASSERT1(argv);
  ASSERT1(guid);
  ASSERT1(is_machine);
  ASSERT1(is_update_check_only);
  ASSERT1(timeout);
  if (argc < 3 || argc > 5) {
    return false;
  }
  *guid = argv[1];

  // Verify that the guid is valid.
  GUID parsed;
  if (FAILED(StringToGuidSafe(*guid, &parsed))) {
    return false;
  }

  *is_machine = !!_ttoi(argv[2]);

  if (argc >= 4) {
    *is_update_check_only = !!_ttoi(argv[3]);
  } else {
    *is_update_check_only = false;
  }

  if (argc >= 5) {
    *timeout = _ttoi(argv[4]);
    if (*timeout == 0) {
      return false;
    }
  } else {
    *timeout = 60;
  }

  return true;
}


DWORD SetTokenIntegrityLevelMedium(HANDLE token) {
  PSID medium_sid = NULL;
  if (!::ConvertStringSidToSid(SDDL_ML_MEDIUM, &medium_sid)) {
    return ::GetLastError();
  }

  TOKEN_MANDATORY_LABEL label = {0};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = medium_sid;

  size_t size = sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(medium_sid);
  BOOL success = ::SetTokenInformation(token, TokenIntegrityLevel, &label,
                                       static_cast<DWORD>(size));
  DWORD result = success ? ERROR_SUCCESS : ::GetLastError();
  ::LocalFree(medium_sid);
  return result;
}

// Reads the Proxy information for the given interface from HKCU, and registers
// it with COM.
HRESULT RegisterHKCUPSClsid(IID iid,
                            HMODULE* proxy_module,
                            DWORD* revoke_cookie) {
  ASSERT1(proxy_module);
  ASSERT1(revoke_cookie);
  *proxy_module = NULL;
  *revoke_cookie = 0;

  const TCHAR* const hkcu_classes_key = _T("HKCU\\Software\\Classes\\");

  // Get the registered proxy for the interface.
  CString interface_proxy_clsid_key;
  interface_proxy_clsid_key.Format(_T("%sInterface\\%s\\ProxyStubClsid32"),
                                   hkcu_classes_key, GuidToString(iid));
  CString proxy_clsid32_value;
  HRESULT hr = RegKey::GetValue(interface_proxy_clsid_key,
                        NULL,
                        &proxy_clsid32_value);
  if (FAILED(hr)) {
    wprintf(_T("RegKey::GetValue failed [%s][0x%x]\n"),
            interface_proxy_clsid_key.GetString(), hr);
    return hr;
  }

  // Get the location of the proxy/stub DLL.
  CString proxy_server32_entry;
  proxy_server32_entry.Format(_T("%sClsid\\%s\\InprocServer32"),
                              hkcu_classes_key, proxy_clsid32_value);
  CString hkcu_proxy_dll_path;
  hr = RegKey::GetValue(proxy_server32_entry,
                        NULL,
                        &hkcu_proxy_dll_path);
  if (FAILED(hr)) {
    wprintf(_T("RegKey::GetValue failed [%s][0x%x]\n"),
            proxy_server32_entry.GetString(), hr);
    return hr;
  }

  // Get the proxy/stub class object.
  typedef HRESULT (STDAPICALLTYPE *DllGetClassObjectTypedef)(REFCLSID clsid,
                                                             REFIID iid,
                                                             void** ptr);
  *proxy_module = ::LoadLibrary(hkcu_proxy_dll_path);
  DllGetClassObjectTypedef fn = NULL;
  if (!GPA(*proxy_module, "DllGetClassObject", &fn)) {
    hr = HRESULT_FROM_WIN32(::GetLastError());
    wprintf(_T("GetProcAddress DllGetClassObject failed [0x%x]\n"), hr);
    return hr;
  }
  CComPtr<IPSFactoryBuffer> fb;
  GUID proxy_clsid;
  hr = StringToGuidSafe(proxy_clsid32_value, &proxy_clsid);
  if (FAILED(hr)) {
    return hr;
  }
  hr = (*fn)(proxy_clsid, IID_IPSFactoryBuffer, reinterpret_cast<void**>(&fb));
  if (FAILED(hr)) {
    wprintf(_T("DllGetClassObject failed [0x%x]\n"), hr);
    return hr;
  }

  // Register the proxy/stub class object.
  hr = ::CoRegisterClassObject(proxy_clsid, fb, CLSCTX_INPROC_SERVER,
                               REGCLS_MULTIPLEUSE, revoke_cookie);
  if (FAILED(hr)) {
    wprintf(_T("CoRegisterClassObject failed [0x%x]\n"), hr);
    return hr;
  }

  // Relate the interface with the proxy/stub, so COM does not do a lookup when
  // unmarshaling the interface.
  hr = ::CoRegisterPSClsid(iid, proxy_clsid);
  if (FAILED(hr)) {
    wprintf(_T("CoRegisterPSClsid failed [0x%x]\n"), hr);
    return hr;
  }

  return S_OK;
}

// A helper class for clients of the Omaha on-demand out-of-proc COM server.
// An instance of this class is typically created on the stack. The class does
// nothing for cases where the OS is not Vista RTM with UAC off.
// This class does the following:
// * Calls CoInitializeSecurity with cloaking set to dynamic. This makes COM
//   use the thread token instead of the process token.
// * Impersonates and sets the thread token to medium integrity. This allows for
//   out-of-proc HKCU COM server activation.
// * Reads and registers per-user proxies for the interfaces that on-demand
//   exposes.
class VistaProxyRegistrar {
 public:
  VistaProxyRegistrar()
      : googleupdate_cookie_(0),
        jobobserver_cookie_(0),
        progresswndevents_cookie_(0),
        is_impersonated(false) {
    HRESULT hr = VistaProxyRegistrarImpl();
    if (FAILED(hr)) {
      wprintf(_T("VistaProxyRegistrarImpl failed [0x%x]\n"), hr);
    }
  }

  ~VistaProxyRegistrar() {
    if (googleupdate_cookie_) {
      VERIFY_SUCCEEDED(::CoRevokeClassObject(googleupdate_cookie_));
    }

    if (jobobserver_cookie_) {
      VERIFY_SUCCEEDED(::CoRevokeClassObject(jobobserver_cookie_));
    }

    if (progresswndevents_cookie_) {
      VERIFY_SUCCEEDED(::CoRevokeClassObject(progresswndevents_cookie_));
    }

    if (is_impersonated) {
      VERIFY1(::RevertToSelf());
    }
  }

 private:
  HRESULT VistaProxyRegistrarImpl() {
    if (!SystemInfo::IsRunningOnVistaRTM() || !::IsUserAnAdmin()) {
      return S_OK;
    }

    bool is_split_token = false;
    HRESULT hr = vista_util::IsUserRunningSplitToken(&is_split_token);
    if (FAILED(hr)) {
      return hr;
    }
    if (is_split_token) {
      return S_OK;
    }

    // Needs to be called very early on in a process.
    // Turn on dynamic cloaking so COM picks up the impersonated thread token.
    hr = ::CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY,
        NULL,
        EOAC_DYNAMIC_CLOAKING,
        NULL);
    if (FAILED(hr)) {
      wprintf(_T("[CoInitializeSecurity failed][0x%x]"), hr);
      return hr;
    }

    is_impersonated = !!::ImpersonateSelf(SecurityImpersonation);
    if (!is_impersonated) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      wprintf(_T("[main: ImpersonateSelf failed][0x%x]"), hr);
      return hr;
    }

    scoped_handle thread_token;
    if (!::OpenThreadToken(::GetCurrentThread(),
                           TOKEN_ALL_ACCESS,
                           false,
                           address(thread_token))) {
      hr = HRESULT_FROM_WIN32(::GetLastError());
      wprintf(_T("[main: OpenThreadToken failed][0x%x]"), hr);
      return hr;
    }

    DWORD result = SetTokenIntegrityLevelMedium(get(thread_token));
    if (result != ERROR_SUCCESS) {
      wprintf(_T("[main: SetTokenIntegrityLevelMedium failed][0x%x]"), result);
      return HRESULT_FROM_WIN32(result);
    }

    hr = RegisterHKCUPSClsid(__uuidof(IGoogleUpdate),
                             address(googleupdate_library_),
                             &googleupdate_cookie_);
    if (FAILED(hr)) {
      wprintf(_T("RegisterHKCUPSClsid for IGoogleUpdate failed [0x%x]\n"), hr);
      return hr;
    }

    hr = RegisterHKCUPSClsid(__uuidof(IJobObserver),
                             address(jobobserver_library_),
                             &jobobserver_cookie_);
    if (FAILED(hr)) {
      wprintf(_T("RegisterHKCUPSClsid for IJobObserver failed [0x%x]\n"), hr);
      return hr;
    }

    hr = RegisterHKCUPSClsid(__uuidof(IProgressWndEvents),
                             address(progresswndevents_library_),
                             &progresswndevents_cookie_);
    if (FAILED(hr)) {
      wprintf(_T("RegisterHKCUPSClsid for IProgressWndEvents failed [0x%x]\n"),
              hr);
      return hr;
    }

    return S_OK;
  }

 private:
  scoped_library googleupdate_library_;
  scoped_library jobobserver_library_;
  scoped_library progresswndevents_library_;

  DWORD googleupdate_cookie_;
  DWORD jobobserver_cookie_;
  DWORD progresswndevents_cookie_;
  bool is_impersonated;
};

int DoMain(int argc, TCHAR* argv[]) {
  CString guid;
  bool is_machine = false;
  bool is_update_check_only = false;
  int timeout = 60;
  if (!ParseParams(argc, argv, &guid, &is_machine,
                   &is_update_check_only, &timeout)) {
    wprintf(_T("Usage: performondemand.exe {GUID} {is_machine: 0|1} ")
            _T("[is_update_check_only=0] [timeout=60]\n"));
    return -1;
  }
  wprintf(_T("GUID: %s\n"), guid.GetString());

  CComModule module;
  scoped_co_init com_apt;
  VistaProxyRegistrar registrar;

  CComObject<JobObserver>* job_observer;
  HRESULT hr = CComObject<JobObserver>::CreateInstance(&job_observer);
  if (!SUCCEEDED(hr)) {
    wprintf(_T("CComObject<JobObserver>::CreateInstance failed [0x%x]\n"), hr);
    return -1;
  }
  CComPtr<IJobObserver> job_holder(job_observer);

  CComPtr<IGoogleUpdate> on_demand;
  hr = on_demand.CoCreateInstance(is_machine ?
                                  __uuidof(OnDemandMachineAppsClass) :
                                  __uuidof(OnDemandUserAppsClass));
  if (!SUCCEEDED(hr)) {
    wprintf(_T("Could not create COM instance [0x%x]\n"), hr);
    return -1;
  }

  if (is_update_check_only) {
    hr = on_demand->CheckForUpdate(guid, job_observer);
  } else {
    hr = on_demand->Update(guid, job_observer);
  }

  if (!SUCCEEDED(hr)) {
    wprintf(_T("on_demand->%sUpdate failed [0x%x]\n"),
            is_update_check_only ? _T("CheckFor") : _T(""), hr);
    return -1;
  }

  // Main message loop:
  MSG msg;
  SYSTEMTIME start_system_time = {0};
  SYSTEMTIME current_system_time = {0};
  ::GetSystemTime(&start_system_time);
  CTime start_time(start_system_time);
  CTimeSpan timeout_period(0, 0, 0, timeout);

  while (::GetMessage(&msg, NULL, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
    ::GetSystemTime(&current_system_time);
    CTime current_time(current_system_time);
    CTimeSpan elapsed_time = current_time - start_time;
    if (timeout_period < elapsed_time) {
      wprintf(_T("Timed out.\n"));
      // TODO(omaha): Right now the timeout does correctly break, but then
      // the COM interactions continue on to completion.
      break;
    }
  }
  int ret_val = job_observer->observed;

  return ret_val;
}

}  // namespace omaha

int _tmain(int argc, TCHAR* argv[]) {
  return omaha::DoMain(argc, argv);
}

