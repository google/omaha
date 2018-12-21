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

#include "omaha/plugins/update/activex/update3web_control.h"
#include <objbase.h>
#include <objidl.h>
#include "omaha/base/error.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/update3_utils.h"
#include "omaha/common/webplugin_utils.h"
#include "omaha/goopdate/app_manager.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

Update3WebControl::Update3WebControl() {
}

// There is a code generation bug in VC8. If a base class with template
// arguments and virtual members is not the first base class with virtual
// methods, the generated code for calling a virtual method on that base class
// will adjust the this pointer one extra time. This results in all sorts of
// strange things happening; typically, the program will crash at a later point.
// To avoid this, all calls to base class methods that have template arguments
// should have all the template arguments specified, since this seems to prevent
// the code generation bug from occuring.

STDMETHODIMP Update3WebControl::createOmahaMachineServerAsync(
    VARIANT_BOOL create_elevated, IDispatch** async_status) {
  ASSERT1(async_status);

  CString url;
  HRESULT hr = SiteLock::GetCurrentBrowserUrl(this, &url);
  if (FAILED(hr)) {
    CORE_LOG(LE, (L"[GetCurrentBrowserUrl failed][0x%08x]", hr));
    return hr;
  }

  if (!site_lock_.InApprovedDomain(url)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!async_status) {
    return E_POINTER;
  }

  CComPtr<ICoCreateAsync> cocreate_async;
  hr = update3_utils::CoCreateWithProxyBlanket(
      __uuidof(CoCreateAsyncClass), &cocreate_async);
  if (FAILED(hr)) {
    CORE_LOG(LE, (L"[CoCreate CoCreateAsyncClass failed][0x%08x]", hr));
    return hr;
  }

  CComPtr<ICoCreateAsyncStatus> status;
  hr = cocreate_async->createOmahaMachineServerAsync(CComBSTR(url),
                                                     create_elevated,
                                                     &status);
  if (FAILED(hr)) {
    CORE_LOG(LE, (L"[CreateInstanceAsync failed][0x%08x]", hr));
    return hr;
  }

  hr = status->QueryInterface(async_status);
  CORE_LOG(L3, (L"[createOmahaMachineServerAsync][0x%p][0x%08x]", this, hr));
  return hr;
}

STDMETHODIMP Update3WebControl::createOmahaUserServer(IDispatch** server) {
  ASSERT1(server);

  CString url;
  HRESULT hr = SiteLock::GetCurrentBrowserUrl(this, &url);
  if (FAILED(hr)) {
    CORE_LOG(LE, (L"[GetCurrentBrowserUrl failed][0x%08x]", hr));
    return hr;
  }

  if (!site_lock_.InApprovedDomain(url)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!server) {
    return E_POINTER;
  }

  CComPtr<IGoogleUpdate3WebSecurity> security;
  hr = update3_utils::CoCreateWithProxyBlanket(
      __uuidof(GoogleUpdate3WebUserClass), &security);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[security.CoCreateWithProxyBlanket failed][0x%x]"), hr));
    return hr;
  }

  hr = security->setOriginURL(CComBSTR(url));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[setOriginURL failed][0x%08x]"), hr));
    return hr;
  }

  hr = security->QueryInterface(server);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[QueryInterface failed][0x%08x]"), hr));
    return hr;
  }

  CORE_LOG(L3, (L"[createOmahaUserServer][0x%p][0x%p]", this, *server));
  return hr;
}

STDMETHODIMP Update3WebControl::getInstalledVersion(BSTR guid_string,
                                                    VARIANT_BOOL is_machine,
                                                    BSTR* version_string) {
  if (!site_lock_.InApprovedDomain(this)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!guid_string || !version_string) {
    return E_POINTER;
  }
  *version_string = NULL;

  CORE_LOG(L2, (_T("[Update3WebControl::getInstalledVersion][%s][%d]"),
                guid_string, is_machine));

  CString version;
  HRESULT hr = GetVersionUsingRegistry(guid_string,
                                       is_machine == VARIANT_TRUE,
                                       &version);
  if (SUCCEEDED(hr)) {
    *version_string = version.AllocSysString();
  }

  return S_OK;
}

#if 0
// TODO(omaha3): Not using this method for now. CoCreation of
// GoogleUpdate3WebMachineClass can block, and should be using the async
// creation pattern aka createOmahaMachineServerAsync.
HRESULT Update3WebControl::GetVersionUsingCOMServer(const TCHAR* guid_string,
                                                    bool is_machine,
                                                    CString* version_string) {
  CORE_LOG(L2, (_T("[GoopdateCtrl::GetVersionUsingCOMServer][%s][%d]"),
                guid_string, is_machine));
  ASSERT1(guid_string);
  ASSERT1(version_string);

  CComPtr<IGoogleUpdate3Web> update3web;
  HRESULT hr = update3_utils::CoCreateWithProxyBlanket(
      is_machine ? __uuidof(GoogleUpdate3WebMachineClass) :
                   __uuidof(GoogleUpdate3WebUserClass),
      &update3web);
  if (FAILED(hr)) {
    CORE_LOG(LE,
             (_T("[update3web.CoCreateWithProxyBlanket failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IDispatch> app_bundle_dispatch;
  hr = update3web->createAppBundleWeb(&app_bundle_dispatch);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[update3web.createAppBundleWeb failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IAppBundleWeb> app_bundle_web;
  hr = app_bundle_dispatch->QueryInterface(&app_bundle_web);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[QueryInterface for IAppBundleWeb failed][0x%x]"), hr));
    return hr;
  }

  hr = app_bundle_web->initialize();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[initialize fail][0x%x]"), hr));
    return hr;
  }

  hr = app_bundle_web->createInstalledApp(CComBSTR(guid_string));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[createInstalledApp fail][%s][0x%x]"), guid_string, hr));
    return hr;
  }

  CComPtr<IDispatch> app_dispatch;
  hr = app_bundle_web->get_appWeb(0, &app_dispatch);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_appWeb failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IAppWeb> app_web;
  hr = app_dispatch->QueryInterface(&app_web);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[QueryInterface for IAppWeb failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IDispatch> app_version_dispatch;
  hr = app_web->get_currentVersionWeb(&app_version_dispatch);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_currentVersionWeb failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IAppVersionWeb> app_version_web;
  hr = app_version_dispatch->QueryInterface(&app_version_web);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[QueryInterface for IAppVersionWeb failed][0x%x]"), hr));
    return hr;
  }

  CComBSTR version;
  hr = app_version_web->get_version(&version);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_version failed][0x%x]"), hr));
    return hr;
  }

  *version_string = version;
  CORE_LOG(L2, (_T("[Update3WebControl::GetVersionUsingCOMServer][%s][%d][%s]"),
                guid_string, is_machine, *version_string));
  return S_OK;
}
#endif

HRESULT Update3WebControl::GetVersionUsingRegistry(const TCHAR* guid_string,
                                                   bool is_machine,
                                                   CString* version_string) {
  CORE_LOG(L2, (_T("[GoopdateCtrl::GetVersionUsingRegistry][%s][%d]"),
                guid_string, is_machine));
  ASSERT1(guid_string);
  ASSERT1(version_string);

  GUID app_guid = GUID_NULL;
  HRESULT hr = StringToGuidSafe(guid_string, &app_guid);
  if (FAILED(hr)) {
    return hr;
  }

  return AppManager::ReadAppVersionNoLock(is_machine, app_guid, version_string);
}

HRESULT Update3WebControl::crossInstall(BSTR extra_args) {
  ASSERT1(extra_args);

  if (!site_lock_.InApprovedDomain(this)) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!extra_args || !extra_args[0]) {
    return E_INVALIDARG;
  }

  CORE_LOG(L2, (_T("[Update3WebControl::crossInstall][%s]"), extra_args));

  // Build the full command line as it should eventually be run.  (This command
  // line will be escaped, including the /install, and stowed in a /pi later.)

  CommandLineBuilder inner_builder(COMMANDLINE_MODE_INSTALL);
  inner_builder.set_extra_args(CString(extra_args));
  CString inner_cmd_line_args = inner_builder.GetCommandLineArgs();

  HRESULT hr = webplugin_utils::IsLanguageSupported(inner_cmd_line_args);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[IsLanguageSupported failed][0x%08x]"), hr));
    return hr;
  }

  CString browser_url;
  hr = site_lock_.GetCurrentBrowserUrl(this, &browser_url);
  if (FAILED(hr)) {
    return hr;
  }

  CString url_domain;
  hr = SiteLock::GetUrlDomain(browser_url, &url_domain);
  if (FAILED(hr)) {
    return hr;
  }

  // Build the outer command line using /pi.

  CString url_domain_encoded;
  CString cmd_line_encoded;
  hr = StringEscape(url_domain, true, &url_domain_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  hr = StringEscape(inner_cmd_line_args, true, &cmd_line_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  CommandLineBuilder outer_builder(COMMANDLINE_MODE_WEBPLUGIN);
  outer_builder.set_webplugin_url_domain(url_domain_encoded);
  outer_builder.set_webplugin_args(cmd_line_encoded);
  outer_builder.set_install_source(kCmdLineInstallSource_Update3Web);
  CString final_cmd_line_args = outer_builder.GetCommandLineArgs();

  CORE_LOG(L2, (_T("[Update3WebControl::crossInstall]")
                _T("[Final command line params: %s]"),
                final_cmd_line_args));

  // Spawn a gu process.

  scoped_process process_goopdate;

  hr = goopdate_utils::StartGoogleUpdateWithArgs(is_machine(),
                                                 final_cmd_line_args,
                                                 address(process_goopdate));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Update3WebControl::crossInstall]")
                  _T("[Failed StartGoogleUpdateWithArgs][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT Update3WebControl::launchAppCommand(BSTR, VARIANT_BOOL, BSTR) {
  return E_NOTIMPL;
}

Update3WebControl::~Update3WebControl() {
}

}  // namespace omaha
