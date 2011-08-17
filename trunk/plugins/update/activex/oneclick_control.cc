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
// Implementation of the OneClick Plugin.

#include "omaha/plugins/update/activex/oneclick_control.h"

#include <dispex.h>
#include <atlsafe.h>
#include <atlbase.h>
#include <atlcom.h>

#include "omaha/base/error.h"
#include "omaha/base/exception_barrier.h"
#include "omaha/base/file.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/string.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/webplugin_utils.h"
#include "omaha/goopdate/app_command.h"
#include "omaha/goopdate/app_manager.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

OneClickControl::OneClickControl() {
  CORE_LOG(L2, (_T("[OneClickControl::OneClickControl]")));
}

OneClickControl::~OneClickControl() {
  CORE_LOG(L2, (_T("[OneClickControl::~OneClickControl]")));
}

STDMETHODIMP OneClickControl::Install(BSTR cmd_line_args,
                                      VARIANT* success_callback,
                                      VARIANT* failure_callback) {
  ASSERT1(cmd_line_args && cmd_line_args[0]);
  ASSERT1(VariantIsValidCallback(success_callback));
  ASSERT1(VariantIsValidCallback(failure_callback));

  if (!site_lock_.InApprovedDomain(this)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!cmd_line_args ||
      !cmd_line_args[0] ||
      !VariantIsValidCallback(success_callback) ||
      !VariantIsValidCallback(failure_callback) ) {
    return E_INVALIDARG;
  }

  CORE_LOG(L2, (_T("[OneClickControl::Install][cmd_line \"%s\"]"),
                CW2CT(cmd_line_args)));

  HRESULT hr = DoInstall(CString(cmd_line_args));
  if (SUCCEEDED(hr)) {
    InvokeJavascriptCallback(success_callback, NULL);
  } else {
    CORE_LOG(LE, (_T("[DoOneClickInstallInternal failed][0x%08x]"), hr));
    InvokeJavascriptCallback(failure_callback, &hr);
  }

  // Return success in all cases. The failure callback has already been called
  // above, and we don't want to cause a failure path to be called again when
  // the JavaScript catches the exception.

  return S_OK;
}

STDMETHODIMP OneClickControl::Install2(BSTR extra_args) {
  ASSERT1(extra_args && extra_args[0]);

  if (!site_lock_.InApprovedDomain(this)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!extra_args || !extra_args[0]) {
    return E_INVALIDARG;
  }

  CORE_LOG(L2, (_T("[OneClickControl::Install2][extra_args \"%s\"]"),
                CW2CT(extra_args)));

  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(CString(extra_args));
  return DoInstall(builder.GetCommandLineArgs());
}

STDMETHODIMP OneClickControl::GetInstalledVersion(BSTR guid_string,
                                                  VARIANT_BOOL is_machine,
                                                  BSTR* version_string) {
  if (!site_lock_.InApprovedDomain(this)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!guid_string || !version_string) {
    return E_POINTER;
  }
  *version_string = NULL;

  CORE_LOG(L2, (_T("[OneClickControl::GetInstalledVersion][%s][%d]"),
                guid_string, is_machine));

  CString version;
  HRESULT hr = DoGetInstalledVersion(guid_string,
                                     is_machine == VARIANT_TRUE,
                                     &version);
  if (SUCCEEDED(hr)) {
    *version_string = version.AllocSysString();
  }

  return S_OK;
}

STDMETHODIMP OneClickControl::GetOneClickVersion(long* version) {  // NOLINT
  ASSERT1(version);

  if (!site_lock_.InApprovedDomain(this)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  CORE_LOG(L2, (_T("[OneClickControl::GetOneClickVersion]")));

  if (!version) {
    return E_POINTER;
  }

  *version = atoi(ONECLICK_PLUGIN_VERSION_ANSI);  // NOLINT
  return S_OK;
}

STDMETHODIMP OneClickControl::LaunchAppCommand(BSTR app_guid,
                                               VARIANT_BOOL is_machine,
                                               BSTR cmd_id) {
  if (!site_lock_.InApprovedDomain(this)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  CORE_LOG(L2, (_T("[OneClickControl::LaunchAppCommand]")));

  ExceptionBarrier barrier;

  CComPtr<IOneClickProcessLauncher> process_launcher;
  HRESULT hr = E_UNEXPECTED;

  hr = process_launcher.CoCreateInstance(
      is_machine ?
          CLSID_OneClickMachineProcessLauncherClass :
          CLSID_OneClickUserProcessLauncherClass);

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[OneClickControl::LaunchAppCommand]")
                  _T("[Failed to CoCreate OneClickMachine/User ")
                  _T("ProcessLauncher implementation: 0x%x"),
                  hr));
    return hr;
  }

  return process_launcher->LaunchAppCommand(app_guid, cmd_id);
}

HRESULT OneClickControl::DoGetInstalledVersion(const TCHAR* guid_string,
                                               bool is_machine,
                                               CString* version_string) {
  ASSERT1(guid_string);
  ASSERT1(version_string);

  GUID app_guid = GUID_NULL;
  HRESULT hr = StringToGuidSafe(guid_string, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }

  return AppManager::ReadAppVersionNoLock(is_machine, app_guid, version_string);
}

HRESULT OneClickControl::DoInstall(const TCHAR* cmd_line_args) {
  ASSERT1(cmd_line_args);

  CORE_LOG(L2, (_T("[OneClickControl::DoInstall][%s]"), cmd_line_args));

#ifdef _DEBUG
  // If the args are exactly __DIRECTNOTIFY__ then just fire the event
  // out of this thread.  This allows for easy testing of the
  // browser interface without requiring launch of
  // google_update.exe.
  if (0 == _wcsicmp(L"__DIRECTNOTIFY__", cmd_line_args)) {
    return S_OK;
  }
#endif

  CString browser_url;
  HRESULT hr = site_lock_.GetCurrentBrowserUrl(this, &browser_url);
  if (FAILED(hr)) {
    return hr;
  }

  CString url_domain;
  hr = SiteLock::GetUrlDomain(browser_url, &url_domain);
  if (FAILED(hr)) {
    return hr;
  }

  CString url_domain_encoded;
  CString cmd_line_args_encoded;
  hr = StringEscape(url_domain, false, &url_domain_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  hr = StringEscape(cmd_line_args, false, &cmd_line_args_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  CommandLineBuilder builder(COMMANDLINE_MODE_WEBPLUGIN);
  builder.set_webplugin_url_domain(url_domain_encoded);
  builder.set_webplugin_args(cmd_line_args_encoded);
  builder.set_install_source(kCmdLineInstallSource_OneClick);
  CString final_cmd_line_args = builder.GetCommandLineArgs();

  CORE_LOG(L2, (_T("[OneClickControl::DoInstall]")
                _T("[Final command line params: %s]"),
                final_cmd_line_args));

  scoped_process process_goopdate;

  hr = goopdate_utils::StartGoogleUpdateWithArgs(
          goopdate_utils::IsRunningFromOfficialGoopdateDir(true),
          final_cmd_line_args,
          address(process_goopdate));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[OneClickControl::DoInstall]")
                  _T("[Failed StartGoogleUpdateWithArgs][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

bool OneClickControl::VariantIsValidCallback(const VARIANT* callback) {
  return callback &&
         (callback->vt == VT_NULL ||
          callback->vt == VT_EMPTY ||
          (callback->vt == VT_DISPATCH && callback->pdispVal));
}

HRESULT OneClickControl::InvokeJavascriptCallback(VARIANT* callback,
                                                  const HRESULT* opt_param) {
  if (!callback || callback->vt == VT_NULL || callback->vt == VT_EMPTY) {
    return S_FALSE;
  }

  if (callback->vt != VT_DISPATCH || !callback->pdispVal) {
    return E_FAIL;
  }

  const DISPID kDispId0 = 0;
  DISPPARAMS dispparams = {0};

  CComQIPtr<IDispatchEx> dispatchex = callback->pdispVal;
  if (dispatchex) {
    DISPID disp_this = DISPID_THIS;
    VARIANT var[2];
    var[0].vt = VT_DISPATCH;
    var[0].pdispVal = dispatchex;
    if (opt_param) {
      var[1].vt = VT_I4;
      var[1].intVal = *opt_param;
    }

    dispparams.rgvarg = var;
    dispparams.rgdispidNamedArgs = &disp_this;
    dispparams.cNamedArgs = 1;
    dispparams.cArgs = opt_param ? 2 : 1;

    return dispatchex->InvokeEx(kDispId0, LOCALE_USER_DEFAULT,
                                DISPATCH_METHOD, &dispparams,
                                NULL, NULL, NULL);
  } else {
    // Fallback on IDispatch if needed.  (This route will be used for NPAPI
    // functions wrapped via NPFunctionHost.)

    UINT arg_err = 0;
    VARIANT var[1];

    if (opt_param) {
      var[0].vt = VT_I4;
      var[0].intVal = *opt_param;
      dispparams.rgvarg = var;
      dispparams.cArgs = 1;
    }

    return callback->pdispVal->Invoke(kDispId0,
                                      IID_NULL,
                                      LOCALE_SYSTEM_DEFAULT,
                                      DISPATCH_METHOD,
                                      &dispparams,
                                      NULL,
                                      NULL,
                                      &arg_err);
  }
}

// This works during Setup because IsRunningFromOfficialGoopdateDir checks the
// location of the DLL not the exe, which is running from a temp location.
CString OneClickControl::GetGoopdateShellPathForRegMap() {
  ASSERT1(goopdate_utils::IsRunningFromOfficialGoopdateDir(false) ||
          goopdate_utils::IsRunningFromOfficialGoopdateDir(true));
  return goopdate_utils::BuildGoogleUpdateExeDir(
      goopdate_utils::IsRunningFromOfficialGoopdateDir(true));
}

}  // namespace omaha

// 4505: unreferenced local function has been removed
#pragma warning(disable : 4505)

