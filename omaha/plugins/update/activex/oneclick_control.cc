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
#include "omaha/base/file.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/update3_utils.h"
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

  const TCHAR kExtraArgsInstallPrefix[] = _T("/install \"");
  const TCHAR kExtraArgsSuffix[] = _T("\"");
  if (!String_StartsWith(cmd_line_args, kExtraArgsInstallPrefix, false) ||
      !String_EndsWith(cmd_line_args, kExtraArgsSuffix, false)) {
    return E_INVALIDARG;
  }

  CORE_LOG(L2, (_T("[OneClickControl::Install][cmd_line \"%s\"]"),
                CW2CT(cmd_line_args)));

  // We trim cmd_line_args to just the extra args and thunk to Install2().
  CString extra_args(cmd_line_args);
  extra_args = extra_args.Mid(arraysize(kExtraArgsInstallPrefix) - 1);
  extra_args.Delete(extra_args.GetLength() - 1);

  HRESULT hr = Install2(extra_args.AllocSysString());
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

  // To protect against XSS attacks where an arbitrary extra_args could be
  // passed into Install/Install2, BuildWebPluginCommandLine escapes all unsafe
  // characters such as space, slash, double-quotes. ShellExecuteProcess is then
  // passed a command line that is safe to interpret by the command line parser.
  CString final_cmd_line_args;
  hr = webplugin_utils::BuildWebPluginCommandLine(url_domain,
                                                  extra_args,
                                                  &final_cmd_line_args);

  CPath webpluginexe_path(app_util::GetCurrentModuleDirectory());
  VERIFY1(webpluginexe_path.Append(kOmahaWebPluginFileName));

  scoped_process process_webpluginexe;
  hr = System::ShellExecuteProcess(webpluginexe_path,
                                   final_cmd_line_args,
                                   NULL,
                                   address(process_webpluginexe));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[OneClickControl::Install2]")
                  _T("[ShellExecuteProcess failed][%s][%s][0x%x]"),
                  webpluginexe_path, final_cmd_line_args, hr));
    return hr;
  }

  return S_OK;
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

  CComPtr<IOneClickProcessLauncher> process_launcher;
  HRESULT hr = update3_utils::CoCreateWithProxyBlanket(
      is_machine ?
          __uuidof(OneClickMachineProcessLauncherClass) :
          __uuidof(OneClickUserProcessLauncherClass),
      &process_launcher);

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

}  // namespace omaha

// 4505: unreferenced local function has been removed
#pragma warning(disable : 4505)

