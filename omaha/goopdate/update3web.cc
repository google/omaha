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

#include "omaha/goopdate/update3web.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/user_rights.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/update3_utils.h"
#include "omaha/common/lang.h"

namespace omaha {

namespace {

template <typename Base, typename T, typename Z>
HRESULT ComInitHelper(T data, Z** p) {
  *p = NULL;
  CComObject<Base>* object;
  HRESULT hr = CComObject<Base>::CreateInstance(&object);
  if (FAILED(hr)) {
    return hr;
  }
  CComPtr<IUnknown> object_releaser = object;
  hr = object->Init(data);
  if (FAILED(hr)) {
    return hr;
  }
  return object->QueryInterface(IID_PPV_ARGS(p));
}

class ATL_NO_VTABLE AppBundleWeb
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatchImpl<IAppBundleWeb,
                           &__uuidof(IAppBundleWeb),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  AppBundleWeb();
  HRESULT Init(Update3WebBase* update3web);

  DECLARE_NOT_AGGREGATABLE(AppBundleWeb);
  DECLARE_NO_REGISTRY();

  BEGIN_COM_MAP(AppBundleWeb)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IAppBundleWeb)
  END_COM_MAP()

  STDMETHOD(createApp)(BSTR app_id, BSTR brand_code, BSTR language, BSTR ap);
  STDMETHOD(createInstalledApp)(BSTR app_id);
  STDMETHOD(createAllInstalledApps)();
  STDMETHOD(get_displayLanguage)(BSTR* language);
  STDMETHOD(put_displayLanguage)(BSTR language);
  STDMETHOD(put_parentHWND)(ULONG_PTR hwnd);
  STDMETHOD(get_length)(int* number);
  STDMETHOD(get_appWeb)(int index, IDispatch** app_web);
  STDMETHOD(initialize)();
  STDMETHOD(checkForUpdate)();
  STDMETHOD(download)();
  STDMETHOD(install)();
  STDMETHOD(pause)();
  STDMETHOD(resume)();
  STDMETHOD(cancel)();
  STDMETHOD(downloadPackage)(BSTR app_id, BSTR package_name);
  STDMETHOD(get_currentState)(VARIANT* current_state);

 protected:
  virtual ~AppBundleWeb();

 private:
  Update3WebBase* update3web_;
  CComPtr<IAppBundle> app_bundle_;

  DISALLOW_COPY_AND_ASSIGN(AppBundleWeb);
};

class ATL_NO_VTABLE AppWeb
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatchImpl<IAppWeb,
                           &__uuidof(IAppWeb),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  AppWeb();
  HRESULT Init(IApp* app);

  DECLARE_NOT_AGGREGATABLE(AppWeb);
  DECLARE_NO_REGISTRY();

  BEGIN_COM_MAP(AppWeb)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IAppWeb)
  END_COM_MAP()

  STDMETHOD(get_appId)(BSTR* app_id);
  STDMETHOD(get_currentVersionWeb)(IDispatch** current);
  STDMETHOD(get_nextVersionWeb)(IDispatch** next);
  STDMETHOD(get_command)(BSTR command_id, IDispatch** command);
  STDMETHOD(cancel)();
  STDMETHOD(get_currentState)(IDispatch** current_state);
  STDMETHOD(launch)();
  STDMETHOD(uninstall)();
  STDMETHOD(get_serverInstallDataIndex)(BSTR* language);
  STDMETHOD(put_serverInstallDataIndex)(BSTR language);

 protected:
  virtual ~AppWeb();

 private:
  CComPtr<IApp> app_;

  DISALLOW_COPY_AND_ASSIGN(AppWeb);
};

class ATL_NO_VTABLE AppCommandWeb
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatchImpl<IAppCommandWeb,
                           &__uuidof(IAppCommandWeb),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  AppCommandWeb();
  HRESULT Init(IAppCommand2* app_command);

  DECLARE_NOT_AGGREGATABLE(AppCommandWeb);
  DECLARE_NO_REGISTRY();

  BEGIN_COM_MAP(AppCommandWeb)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IAppCommandWeb)
  END_COM_MAP()

  STDMETHOD(get_status)(UINT* status);
  STDMETHOD(get_exitCode)(DWORD* exit_code);
  STDMETHOD(get_output)(BSTR* output);
  STDMETHOD(execute)(VARIANT arg1,
                     VARIANT arg2,
                     VARIANT arg3,
                     VARIANT arg4,
                     VARIANT arg5,
                     VARIANT arg6,
                     VARIANT arg7,
                     VARIANT arg8,
                     VARIANT arg9);

 protected:
  virtual ~AppCommandWeb();

 private:
  CComPtr<IAppCommand2> app_command_;

  DISALLOW_COPY_AND_ASSIGN(AppCommandWeb);
};

class ATL_NO_VTABLE AppVersionWeb
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatchImpl<IAppVersionWeb,
                           &__uuidof(IAppVersionWeb),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  AppVersionWeb();
  HRESULT Init(IAppVersion* app_version);

  DECLARE_NOT_AGGREGATABLE(AppVersionWeb);
  DECLARE_NO_REGISTRY();

  BEGIN_COM_MAP(AppVersionWeb)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IAppVersionWeb)
  END_COM_MAP()

  STDMETHOD(get_version)(BSTR* version);
  STDMETHOD(get_packageCount)(long* count);  // NOLINT
  STDMETHOD(get_packageWeb)(long index, IDispatch** package);  // NOLINT

 protected:
  virtual ~AppVersionWeb();

 private:
  CComPtr<IAppVersion> app_version_;

  DISALLOW_COPY_AND_ASSIGN(AppVersionWeb);
};

HRESULT AppBundleWeb::Init(Update3WebBase* update3web) {
  ASSERT1(update3web);

  update3web_ = update3web;
  update3web_->AddRef();

  HRESULT hr = update3_utils::CreateAppBundle(update3web_->omaha_server(),
                                              &app_bundle_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateAppBundle failed][0x%x]"), hr));
    return hr;
  }

  // ::CoSetProxyBlanket() settings are per proxy. For Update3Web, after
  // unmarshaling the interface, we need to set the blanket on this new proxy.
  // The proxy blanket on the IAppBundle interface are set explicitly for
  // Update3Web, because Update3Web is a unique case of being a COM server as
  // well as a COM client. The default security settings set for the Update3Web
  // COM server are more restrictive and rightly so, as compared to the settings
  // that we set for a COM client such as the Omaha3 UI. Hence the need to
  // explicitly set the proxy blanket settings and lower the security
  // requirements only when calling out on this interface.
  hr = update3_utils::SetProxyBlanketAllowImpersonate(app_bundle_);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_->put_originURL(CComBSTR(update3web_->origin_url()));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[put_originURL failed][0x%x]"), hr));
    return hr;
  }

  hr = app_bundle_->put_displayLanguage(
      CComBSTR(lang::GetLanguageForProcess(CString())));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[put_displayLanguage failed][0x%x]"), hr));
    return hr;
  }

  // TODO(omaha3): Expose setting the display name to the plugin client.
  hr = app_bundle_->put_displayName(CComBSTR(_T("App")));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[put_displayName failed][0x%x]"), hr));
    return hr;
  }

  hr = app_bundle_->put_installSource(
      CComBSTR(kCmdLineInstallSource_Update3Web));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[put_installSource failed][0x%x]"), hr));
    return hr;
  }

  if (update3web_->is_machine_install()) {
    hr = app_bundle_->put_altTokens(
        reinterpret_cast<ULONG_PTR>(update3web_->impersonation_token()),
        reinterpret_cast<ULONG_PTR>(update3web_->primary_token()),
        ::GetCurrentProcessId());
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[put_altTokens failed][0x%x]"), hr));
      return hr;
    }
  }

  return S_OK;
}

STDMETHODIMP AppBundleWeb::get_displayLanguage(BSTR* language) {
  return app_bundle_->get_displayLanguage(language);
}

STDMETHODIMP AppBundleWeb::put_displayLanguage(BSTR language) {
  return app_bundle_->put_displayLanguage(language);
}

STDMETHODIMP AppBundleWeb::put_parentHWND(ULONG_PTR hwnd) {
  return app_bundle_->put_parentHWND(hwnd);
}

STDMETHODIMP AppBundleWeb::get_length(int* number) {
  long long_number = 0;  // NOLINT(runtime/int)
  HRESULT hr = app_bundle_->get_Count(&long_number);
  *number = long_number;
  return hr;
}

STDMETHODIMP AppBundleWeb::get_appWeb(int index, IDispatch** app_web) {
  *app_web = NULL;

  CComPtr<IApp> app;
  HRESULT hr = update3_utils::GetApp(app_bundle_, index, &app);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetApp failed][0x%x]"), hr));
    return hr;
  }

  return ComInitHelper<AppWeb>(app.p, app_web);
}

AppBundleWeb::AppBundleWeb() : update3web_(NULL) {
}

STDMETHODIMP AppBundleWeb::createApp(BSTR app_id,
                                     BSTR brand_code,
                                     BSTR language,
                                     BSTR ap) {
  CComPtr<IApp> app;
  HRESULT hr = update3_utils::CreateApp(app_id, app_bundle_, &app);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateApp failed][0x%x]"), hr));
    return hr;
  }

  hr = app->put_brandCode(brand_code);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app->put_language(language);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app->put_ap(ap);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app->put_isEulaAccepted(VARIANT_TRUE);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_->put_installSource(
      CComBSTR(kCmdLineInstallSource_Update3Web_NewApps));
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

STDMETHODIMP AppBundleWeb::createInstalledApp(BSTR app_id) {
  CComPtr<IApp> app;
  HRESULT hr = update3_utils::CreateInstalledApp(app_id, app_bundle_, &app);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateInstalledApp failed][0x%x]"), hr));
    return hr;
  }

  hr = app_bundle_->put_installSource(
      CComBSTR(kCmdLineInstallSource_Update3Web_OnDemand));
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT AppBundleWeb::createAllInstalledApps() {
  HRESULT hr = update3_utils::CreateAllInstalledApps(app_bundle_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateAllInstalledApps failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

AppBundleWeb::~AppBundleWeb() {
  update3web_->Release();
}

STDMETHODIMP AppBundleWeb::initialize() {
  return app_bundle_->initialize();
}

STDMETHODIMP AppBundleWeb::checkForUpdate() {
  return app_bundle_->checkForUpdate();
}

STDMETHODIMP AppBundleWeb::download() {
  return app_bundle_->download();
}

STDMETHODIMP AppBundleWeb::install() {
  if (update3web_->is_machine_install() &&
      !UserRights::TokenIsAdmin(update3web_->impersonation_token())) {
    CORE_LOG(LE, (_T("[Need to be an admin to call this method]")));
    return E_ACCESSDENIED;
  }

  return app_bundle_->install();
}

STDMETHODIMP AppBundleWeb::pause() {
  return app_bundle_->pause();
}

STDMETHODIMP AppBundleWeb::resume() {
  return app_bundle_->resume();
}

STDMETHODIMP AppBundleWeb::cancel() {
  if (update3web_->is_machine_install() &&
      !UserRights::TokenIsAdmin(update3web_->impersonation_token())) {
    CORE_LOG(LE, (_T("[Need to be an admin to cancel]")));
    return E_ACCESSDENIED;
  }

  return app_bundle_->stop();
}

STDMETHODIMP AppBundleWeb::downloadPackage(BSTR app_id, BSTR package_name) {
  CORE_LOG(L1, (_T("[AppBundleWeb::downloadPackage][%s][%s]"),
                app_id, package_name));

  HRESULT hr = app_bundle_->put_installSource(
      CComBSTR(kCmdLineInstallSource_Update3Web_Components));
  if (FAILED(hr)) {
    return hr;
  }

  return app_bundle_->downloadPackage(app_id, package_name);
}

STDMETHODIMP AppBundleWeb::get_currentState(VARIANT* current_state) {
  return app_bundle_->get_currentState(current_state);
}

AppWeb::AppWeb() {
}

HRESULT AppWeb::Init(IApp* app) {
  app_ = app;
  return S_OK;
}

STDMETHODIMP AppWeb::get_appId(BSTR* app_id) {
  ASSERT1(app_id);
  return app_->get_appId(app_id);
}

STDMETHODIMP AppWeb::get_currentVersionWeb(IDispatch** current) {
  ASSERT1(current);
  *current = NULL;

  CComPtr<IAppVersion> app_version;
  HRESULT hr = update3_utils::GetCurrentAppVersion(app_, &app_version);
  if (FAILED(hr)) {
    return hr;
  }

  return ComInitHelper<AppVersionWeb>(app_version.p, current);
}

STDMETHODIMP AppWeb::get_nextVersionWeb(IDispatch** next) {
  ASSERT1(next);
  *next = NULL;

  CComPtr<IAppVersion> app_version;
  HRESULT hr = update3_utils::GetNextAppVersion(app_, &app_version);
  if (FAILED(hr)) {
    return hr;
  }

  return ComInitHelper<AppVersionWeb>(app_version.p, next);
}

STDMETHODIMP AppWeb::get_command(BSTR command_id, IDispatch** command) {
  ASSERT1(command);
  *command = NULL;

  CComPtr<IAppCommand2> app_command;
  HRESULT hr = update3_utils::GetAppCommand(app_, command_id, &app_command);
  if (FAILED(hr)) {
    return hr;
  }
  if (hr == S_FALSE) {
    return S_FALSE;
  }
  ASSERT1(app_command);

  VARIANT_BOOL is_web_accessible;
  hr = app_command->get_isWebAccessible(&is_web_accessible);
  if (FAILED(hr)) {
    return hr;
  }
  if (!is_web_accessible) {
    return E_ACCESSDENIED;
  }

  return ComInitHelper<AppCommandWeb>(app_command.p, command);
}

STDMETHODIMP AppWeb::cancel() {
  return E_NOTIMPL;
}

STDMETHODIMP AppWeb::get_currentState(IDispatch** current_state) {
  *current_state = NULL;
  return app_->get_currentState(current_state);
}

STDMETHODIMP AppWeb::launch() {
  return E_NOTIMPL;
}

STDMETHODIMP AppWeb::uninstall() {
  // This method should check for adminness when implemented.
  return E_NOTIMPL;
}

STDMETHODIMP AppWeb::get_serverInstallDataIndex(BSTR* install_data_index) {
  return app_->get_serverInstallDataIndex(install_data_index);
}

STDMETHODIMP AppWeb::put_serverInstallDataIndex(BSTR install_data_index) {
  return app_->put_serverInstallDataIndex(install_data_index);
}

AppWeb::~AppWeb() {
}

AppCommandWeb::AppCommandWeb() {
}

HRESULT AppCommandWeb::Init(IAppCommand2* app_command) {
  app_command_ = app_command;
  return S_OK;
}

HRESULT AppCommandWeb::get_status(UINT* status) {
  return app_command_->get_status(status);
}

HRESULT AppCommandWeb::get_exitCode(DWORD* exit_code) {
  return app_command_->get_exitCode(exit_code);
}

HRESULT AppCommandWeb::get_output(BSTR* output) {
  return app_command_->get_output(output);
}

HRESULT AppCommandWeb::execute(VARIANT arg1,
                               VARIANT arg2,
                               VARIANT arg3,
                               VARIANT arg4,
                               VARIANT arg5,
                               VARIANT arg6,
                               VARIANT arg7,
                               VARIANT arg8,
                               VARIANT arg9) {
  return app_command_->execute(
      arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
}

AppCommandWeb::~AppCommandWeb() {
}

AppVersionWeb::AppVersionWeb() {
}

HRESULT AppVersionWeb::Init(IAppVersion* app_version) {
  app_version_ = app_version;
  return S_OK;
}

STDMETHODIMP AppVersionWeb::get_version(BSTR* version) {
  return app_version_->get_version(version);
}

STDMETHODIMP AppVersionWeb::get_packageCount(long* count) {  // NOLINT
  return app_version_->get_packageCount(count);
}

STDMETHODIMP AppVersionWeb::get_packageWeb(long index,  // NOLINT
                                           IDispatch** package) {
  UNREFERENCED_PARAMETER(index);
  ASSERT1(package);
  *package = NULL;

  // TODO(omaha3): Implement this after a security review.
  return E_NOTIMPL;
}

AppVersionWeb::~AppVersionWeb() {
}

}  // namespace

HRESULT Update3WebBase::FinalConstruct() {
  HRESULT hr =
      update3_utils::CreateGoogleUpdate3Class(is_machine_, &omaha_server_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Update3WebBase::FinalConstruct failed][0x%x]"), hr));
    return hr;
  }

  if (!is_machine_) {
    return S_OK;
  }

  hr = UserRights::GetCallerToken(&impersonation_token_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetCallerToken failed][0x%x]"), hr));
    return hr;
  }

  if (!impersonation_token_.CreatePrimaryToken(&primary_token_)) {
    hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[CreatePrimaryToken failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

STDMETHODIMP Update3WebBase::createAppBundleWeb(IDispatch** app_bundle_web) {
  ASSERT1(app_bundle_web);

  *app_bundle_web = NULL;

  return ComInitHelper<AppBundleWeb>(this, app_bundle_web);
}

STDMETHODIMP Update3WebBase::setOriginURL(BSTR origin_url) {
  CORE_LOG(L3, (_T("[Update3WebBase::setOriginURL][%s]"), origin_url));

  if (!origin_url || !wcslen(origin_url)) {
    return E_INVALIDARG;
  }

  origin_url_ = origin_url;
  return S_OK;
}

}  // namespace omaha

