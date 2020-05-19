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

#include"omaha/goopdate/app_bundle.h"

#include <atlsafe.h>
#include <intsafe.h>

#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/thread_pool_callback.h"
#include "omaha/base/user_rights.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/lang.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"
#include "omaha/common/web_services_client.h"
#include "omaha/goopdate/app_bundle_state_init.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/goopdate.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/update_request_utils.h"

namespace omaha {

namespace {

const TCHAR* const kDefaultInstallSource = _T("unknown");

}  // namespace

AppBundle::AppBundle(bool is_machine, Model* model)
    : ModelObject(model),
      install_source_(kDefaultInstallSource),
      is_machine_(is_machine),
      is_auto_update_(false),
      send_pings_(true),
      priority_(INSTALL_PRIORITY_HIGH),
      parent_hwnd_(NULL),
      user_work_item_(NULL),
      display_language_(lang::GetDefaultLanguage(is_machine)) {
  CORE_LOG(L3, (_T("[AppBundle::AppBundle][0x%p]"), this));
  app_bundle_state_.reset(new fsm::AppBundleStateInit);

  VERIFY_SUCCEEDED(GetGuid(&request_id_));
}

AppBundle::~AppBundle() {
  CORE_LOG(L3, (_T("[AppBundle::~AppBundle][0x%p]"), this));

  // Destruction of this object is not serialized. The lifetime of AppBundle
  // objects is controlled by the client and multiple objects can destruct at
  // the same time.
  ASSERT1(!model()->IsLockedByCaller());

  if (send_pings_) {
    HRESULT hr = SendPingEventsAsync();
    CORE_LOG(L3, (_T("[SendPingEventsAsync returned 0x%x]"), hr));
  }

  __mutexScope(model()->lock());
  for (size_t i = 0; i < apps_.size(); ++i) {
    delete apps_[i];
  }

  for (size_t i = 0; i < uninstalled_apps_.size(); ++i) {
    delete uninstalled_apps_[i];
  }

  // If the thread running this AppBundle does not exit before the
  // NetworkConfigManager::DeleteInstance() happens in GoopdateImpl::Main, the
  // update_check_client_ destructor will crash. Resetting here explicitly.
  update_check_client_.reset();

  // Garbage-collect everything that has expired, including this object.
  // The model holds weak references to AppBundle objects. Those weak
  // references expire before the destructor for the object runs. Therefore, it
  // is not possible to associate this object with any of the weak references
  // in the model. Those weak references must be garbage collected.
  model()->CleanupExpiredAppBundles();
}

ControllingPtr AppBundle::controlling_ptr() {
  __mutexScope(model()->lock());
  return shared_from_this();
}

bool AppBundle::is_pending_non_blocking_call() const {
  __mutexScope(model()->lock());
  return user_work_item_ != NULL;
}

void AppBundle::set_user_work_item(UserWorkItem* user_work_item) {
  ASSERT(user_work_item, (_T("Use CompleteAsyncCall() instead.")));
  __mutexScope(model()->lock());

  user_work_item_ = user_work_item;
}

HANDLE AppBundle::impersonation_token() const {
  __mutexScope(model()->lock());
  return alt_impersonation_token_.GetHandle() ?
         alt_impersonation_token_.GetHandle() :
         impersonation_token_.GetHandle();
}

HANDLE AppBundle::primary_token() const {
  __mutexScope(model()->lock());
  return alt_primary_token_.GetHandle() ? alt_primary_token_.GetHandle() :
                                          primary_token_.GetHandle();
}

HRESULT AppBundle::CaptureCallerImpersonationToken() {
  __mutexScope(model()->lock());

  if (!is_machine_) {
    return S_OK;
  }

  if (impersonation_token_.GetHandle()) {
    ::CloseHandle(impersonation_token_.Detach());
  }

  HRESULT hr = UserRights::GetCallerToken(&impersonation_token_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CaptureCallerImpersonationToken failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT AppBundle::CaptureCallerPrimaryToken() {
  __mutexScope(model()->lock());

  if (!is_machine_) {
    return S_OK;
  }

  ASSERT1(impersonation_token_.GetHandle());
  if (!UserRights::TokenIsAdmin(impersonation_token_.GetHandle())) {
    ASSERT1(false);
    return E_UNEXPECTED;
  }

  if (primary_token_.GetHandle()) {
    ::CloseHandle(primary_token_.Detach());
  }

  if (!impersonation_token_.CreatePrimaryToken(&primary_token_)) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[CreatePrimaryToken failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

size_t AppBundle::GetNumberOfApps() const {
  __mutexScope(model()->lock());
  return apps_.size();
}

App* AppBundle::GetApp(size_t index) {
  __mutexScope(model()->lock());

  if (index >= GetNumberOfApps()) {
    ASSERT1(false);
    return NULL;
  }

  App* app = apps_[index];
  ASSERT1(app);
  return app;
}

CString AppBundle::FetchAndResetLogText() {
  __mutexScope(model()->lock());

  CString event_log_text;
  for (size_t i = 0; i < apps_.size(); ++i) {
    event_log_text += apps_[i]->FetchAndResetLogText();
  }

  return event_log_text;
}

void AppBundle::BuildPing(std::unique_ptr<Ping>* my_ping) {
  CORE_LOG(L3, (_T("[AppBundle::BuildPing]")));
  ASSERT1(model()->IsLockedByCaller());
  ASSERT1(my_ping);

  std::unique_ptr<Ping> ping(
      new Ping(is_machine_, session_id_, install_source_, request_id_));

  for (size_t i = 0; i != apps_.size(); ++i) {
    if (apps_[i]->is_eula_accepted()) {
      ping->BuildRequest(apps_[i]);
    }
  }

  for (size_t i = 0; i != uninstalled_apps_.size(); ++i) {
    if (uninstalled_apps_[i]->is_eula_accepted()) {
      ping->BuildRequest(uninstalled_apps_[i]);
    }
  }

  my_ping->reset(ping.release());
}

HRESULT AppBundle::BuildAndPersistPing() {
  CORE_LOG(L3, (_T("[AppBundle::BuildAndPersistPing]")));

  std::unique_ptr<Ping> ping;
  BuildPing(&ping);
  return ping->PersistPing();
}

HRESULT AppBundle::SendPingEventsAsync() {
  CORE_LOG(L3, (_T("[AppBundle::SendPingEventsAsync]")));

  scoped_impersonation impersonate_user(impersonation_token());

  if (!ConfigManager::Instance()->CanUseNetwork(is_machine_)) {
    CORE_LOG(L1, (_T("[Ping not sent because network use prohibited]")));
    return S_OK;
  }

  std::unique_ptr<Ping> ping;

  __mutexBlock(model()->lock()) {
    BuildPing(&ping);
  }

  CORE_LOG(L3, (_T("[AppBundle::SendPingEventsAsync][sending ping events]")
                _T("[%d uninstalled apps]"), uninstalled_apps_.size()));

  CAccessToken token;
  if (impersonation_token()) {
    VERIFY_SUCCEEDED(DuplicateTokenIntoCurrentProcess(::GetCurrentProcess(),
                                                       impersonation_token(),
                                                       &token));
  }

  // We Lock the ATL Module here since we want the process to stick around
  // until the newly created threadpool item below starts and also completes
  // execution. The corresponding Unlock of the ATL Module is done at the end
  // of the threadpool proc.
  _pAtlModule->Lock();
  ScopeGuard atl_module_unlock = MakeObjGuard(*_pAtlModule,
                                              &CAtlModule::Unlock);

  using Callback =
    StaticThreadPoolCallBack1<internal::SendPingEventsParameters>;
  HRESULT hr = Goopdate::Instance().QueueUserWorkItem(
      std::make_unique<Callback>(
            &AppBundle::SendPingEvents,
            internal::SendPingEventsParameters(
                ping.get(),
                token.GetHandle())),
            COINIT_MULTITHREADED,
            WT_EXECUTELONGFUNCTION);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[QueueUserWorkItem failed][0x%x]"), hr));
    return hr;
  }

  atl_module_unlock.Dismiss();
  ping.release();
  token.Detach();

  return S_OK;
}

void AppBundle::SendPingEvents(internal::SendPingEventsParameters params) {
  CORE_LOG(L3, (_T("[AppBundle::SendPingEvents]")));

  ON_SCOPE_EXIT_OBJ(*_pAtlModule, &CAtlModule::Unlock);

  std::unique_ptr<Ping> ping(params.ping);
  scoped_handle impersonation_token(params.impersonation_token);
  scoped_impersonation impersonate_user(get(impersonation_token));

  // TODO(Omaha): Add sample to metric_ping_succeeded_ms or
  // metric_ping_failed_ms based on the result of Send().
  HRESULT hr = ping->Send(false);
  CORE_LOG(L3, (_T("[AppBundle::SendPingEvents][0x%x]"), hr));
}

// IAppBundle.
STDMETHODIMP AppBundle::get_displayName(BSTR* display_name) {
  ASSERT1(display_name);
  __mutexScope(model()->lock());
  *display_name = display_name_.AllocSysString();
  return S_OK;
}

STDMETHODIMP AppBundle::put_displayName(BSTR display_name) {
  __mutexScope(model()->lock());
  display_name_ = display_name;
  return S_OK;
}

STDMETHODIMP AppBundle::get_installSource(BSTR* install_source) {
  ASSERT1(install_source);
  __mutexScope(model()->lock());
  *install_source = install_source_.AllocSysString();
  return S_OK;
}

STDMETHODIMP AppBundle::put_installSource(BSTR install_source) {
  __mutexScope(model()->lock());
  install_source_ = install_source;
  return S_OK;
}

STDMETHODIMP AppBundle::get_originURL(BSTR* origin_url) {
  ASSERT1(origin_url);
  __mutexScope(model()->lock());
  *origin_url = origin_url_.AllocSysString();
  return S_OK;
}

STDMETHODIMP AppBundle::put_originURL(BSTR origin_url) {
  __mutexScope(model()->lock());
  origin_url_ = origin_url;
  return S_OK;
}

STDMETHODIMP AppBundle::get_offlineDirectory(BSTR* offline_dir) {
  ASSERT1(offline_dir);
  __mutexScope(model()->lock());
  *offline_dir = offline_dir_.AllocSysString();
  return S_OK;
}

STDMETHODIMP AppBundle::put_offlineDirectory(BSTR offline_dir) {
  CORE_LOG(L3, (_T("[AppBundle::put_offlineDirectory][%s]"), offline_dir));
  __mutexScope(model()->lock());
  offline_dir_ = offline_dir;
  return S_OK;
}

STDMETHODIMP AppBundle::get_sessionId(BSTR* session_id) {
  ASSERT1(session_id);
  __mutexScope(model()->lock());
  *session_id = session_id_.AllocSysString();
  return S_OK;
}

STDMETHODIMP AppBundle::put_sessionId(BSTR session_id) {
  CORE_LOG(L3, (_T("[AppBundle::put_sessionId][%s]"), session_id));
  __mutexScope(model()->lock());
  return app_bundle_state_->put_sessionId(this, session_id);
}

STDMETHODIMP AppBundle::get_sendPings(VARIANT_BOOL* send_pings) {
  ASSERT1(send_pings);
  __mutexScope(model()->lock());
  *send_pings = send_pings_ ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

STDMETHODIMP AppBundle::put_sendPings(VARIANT_BOOL send_pings) {
  CORE_LOG(L3, (_T("[AppBundle::put_send_pings][%d]"),
                static_cast<int>(send_pings)));
  __mutexScope(model()->lock());
  send_pings_ = send_pings != VARIANT_FALSE;
  return S_OK;
}

STDMETHODIMP AppBundle::get_priority(long* priority) {  // NOLINT
  ASSERT1(priority);
  __mutexScope(model()->lock());
  *priority = priority_;
  return S_OK;
}

STDMETHODIMP AppBundle::put_priority(long priority) {  // NOLINT
  if ((priority < INSTALL_PRIORITY_LOW) || (priority > INSTALL_PRIORITY_HIGH)) {
    return E_INVALIDARG;
  }
  __mutexScope(model()->lock());
  priority_ = priority;
  return S_OK;
}

STDMETHODIMP AppBundle::put_altTokens(ULONG_PTR impersonation_token,
                                      ULONG_PTR primary_token,
                                      DWORD caller_proc_id) {
  ASSERT1(impersonation_token);
  ASSERT1(primary_token);
  ASSERT1(caller_proc_id);
  __mutexScope(model()->lock());

  return app_bundle_state_->put_altTokens(this,
                                          impersonation_token,
                                          primary_token,
                                          caller_proc_id);
}

STDMETHODIMP AppBundle::put_parentHWND(ULONG_PTR hwnd) {
  CORE_LOG(L3, (_T("[AppBundle::put_parentHWND][0x%x]"), hwnd));

  __mutexScope(model()->lock());
  parent_hwnd_ = reinterpret_cast<HWND>(hwnd);
  update_check_client_->set_proxy_auth_config(GetProxyAuthConfig());
  return S_OK;
}

CString AppBundle::display_language() const {
  __mutexScope(model()->lock());
  return display_language_;
}

STDMETHODIMP AppBundle::get_displayLanguage(BSTR* language) {
  ASSERT1(language);
  __mutexScope(model()->lock());
  *language = display_language_.AllocSysString();
  return S_OK;
}

STDMETHODIMP AppBundle::put_displayLanguage(BSTR language) {
  __mutexScope(model()->lock());
  if (::SysStringLen(language) == 0) {
    return E_INVALIDARG;
  }

  if (!lang::IsLanguageSupported(language)) {
    return E_INVALIDARG;
  }

  display_language_ = language;
  return S_OK;
}

bool AppBundle::is_machine() const {
  __mutexScope(model()->lock());
  return is_machine_;
}

bool AppBundle::is_auto_update() const {
  __mutexScope(model()->lock());
  return is_auto_update_;
}

void AppBundle::set_is_auto_update(bool is_auto_update) {
  __mutexScope(model()->lock());
  is_auto_update_ = is_auto_update;
}

bool AppBundle::is_offline_install() const {
  __mutexScope(model()->lock());
  return !offline_dir_.IsEmpty();
}

const CString& AppBundle::offline_dir() const {
  __mutexScope(model()->lock());
  return offline_dir_;
}

const CString& AppBundle::session_id() const {
  __mutexScope(model()->lock());
  return session_id_;
}

int AppBundle::priority() const {
  __mutexScope(model()->lock());
  return priority_;
}

ProxyAuthConfig AppBundle::GetProxyAuthConfig() const {
  __mutexScope(model()->lock());
  return ProxyAuthConfig(parent_hwnd_, display_name_);
}

STDMETHODIMP AppBundle::initialize() {
  __mutexScope(model()->lock());

  // Ensure that clients that run as Local System were designed with alt tokens
  // in mind. The alt tokens might not always be a different user, but at least
  // the client considered the need to set the alt tokens.
  // TODO(omaha): The /ua process should not need to call put_altTokens()
  // when there is no logged in user. This may be causing issues on Windows 7.
  bool alt_tokens_set(alt_impersonation_token_.GetHandle() &&
                      alt_primary_token_.GetHandle());
  ASSERT1(!is_machine_ ||
          alt_tokens_set ||
          !UserRights::VerifyCallerIsSystem());

  return app_bundle_state_->Initialize(this);
}

// App is created with is_update=false because the caller is not using
// information about any installed app. It is either a new or over-install.
STDMETHODIMP AppBundle::createApp(BSTR app_id, App** app) {
  CORE_LOG(L1, (_T("[AppBundle::createApp][%s][0x%p]"), app_id, this));
  ASSERT1(app_id);
  ASSERT1(app);

  __mutexScope(model()->lock());

  scoped_impersonation impersonate_user(impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->CreateApp(this, app_id, app);
}

STDMETHODIMP AppBundle::createInstalledApp(BSTR app_id, App** app) {
  CORE_LOG(L1, (_T("[AppBundle::createInstalledApp][%s][0x%p]"), app_id, this));
  ASSERT1(app);

  __mutexScope(model()->lock());


  scoped_impersonation impersonate_user(impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->CreateInstalledApp(this, app_id, app);
}

STDMETHODIMP AppBundle::createAllInstalledApps() {
  CORE_LOG(L1, (_T("[AppBundle::createAllInstalledApps][0x%p]"), this));

  __mutexScope(model()->lock());

  scoped_impersonation impersonate_user(impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->CreateAllInstalledApps(this);
}

STDMETHODIMP AppBundle::get_Count(long* count) {  // NOLINT
  ASSERT1(count);

  __mutexScope(model()->lock());

  const size_t num_apps = apps_.size();
  if (num_apps > LONG_MAX) {
    return E_FAIL;
  }

  *count = static_cast<long>(num_apps);  // NOLINT

  return S_OK;
}

STDMETHODIMP AppBundle::get_Item(long index, App** app) {  // NOLINT
  ASSERT1(app);

  __mutexScope(model()->lock());

  if (index < 0 || static_cast<size_t>(index) >= apps_.size()) {
    return HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);
  }

  *app = apps_[index];
  return S_OK;
}

WebServicesClientInterface* AppBundle::update_check_client() {
  __mutexScope(model()->lock());
  ASSERT1(update_check_client_.get());
  return update_check_client_.get();
}

STDMETHODIMP AppBundle::checkForUpdate() {
  CORE_LOG(L1, (_T("[AppBundle::checkForUpdate][0x%p]"), this));

  __mutexScope(model()->lock());

  scoped_impersonation impersonate_user(impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->CheckForUpdate(this);
}

STDMETHODIMP AppBundle::download() {
  CORE_LOG(L1, (_T("[AppBundle::download][0x%p]"), this));

  __mutexScope(model()->lock());

  scoped_impersonation impersonate_user(impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->Download(this);
}

// Captures the primary token since it is the only function that needs it, and
// to prevent any scenarios where one user sets up a bundle and another installs
// the app(s) with the other user's credentials.
STDMETHODIMP AppBundle::install() {
  CORE_LOG(L1, (_T("[AppBundle::install][0x%p]"), this));

  __mutexScope(model()->lock());

  HRESULT hr = CaptureCallerPrimaryToken();
  if (FAILED(hr)) {
    return hr;
  }

  scoped_impersonation impersonate_user(impersonation_token());
  hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->Install(this);
}

STDMETHODIMP AppBundle::updateAllApps() {
  CORE_LOG(L1, (_T("[AppBundle::updateAllApps][0x%p]"), this));

  __mutexScope(model()->lock());

  scoped_impersonation impersonate_user(impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->UpdateAllApps(this);
}

STDMETHODIMP AppBundle::stop() {
  CORE_LOG(L1, (_T("[AppBundle::stop][0x%p]"), this));

  __mutexScope(model()->lock());

  return app_bundle_state_->Stop(this);
}

STDMETHODIMP AppBundle::pause() {
  CORE_LOG(L1, (_T("[AppBundle::pause][0x%p]"), this));

  __mutexScope(model()->lock());

  return app_bundle_state_->Pause(this);
}

STDMETHODIMP AppBundle::resume() {
  CORE_LOG(L1, (_T("[AppBundle::resume][0x%p]"), this));

  __mutexScope(model()->lock());

  return app_bundle_state_->Resume(this);
}

STDMETHODIMP AppBundle::isBusy(VARIANT_BOOL* is_busy) {
  CORE_LOG(L3, (_T("[AppBundle::isBusy][0x%p]"), this));
  ASSERT1(is_busy);

  __mutexScope(model()->lock());

  *is_busy = IsBusy() ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

STDMETHODIMP AppBundle::downloadPackage(BSTR app_id, BSTR package_name) {
  CORE_LOG(L1, (_T("[AppBundle::downloadPackage][%s][%s]"),
      app_id, package_name));

  __mutexScope(model()->lock());

  scoped_impersonation impersonate_user(impersonation_token());
  HRESULT hr = impersonate_user.result();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Impersonation failed][0x%08x]"), hr));
    return hr;
  }

  return app_bundle_state_->DownloadPackage(this, app_id, package_name);
}

// TODO(omaha3): May need to provide aggregate status. See TODO in IDL file.
STDMETHODIMP AppBundle::get_currentState(VARIANT* current_state) {
  CORE_LOG(L3, (_T("[AppBundle::get_currentState][0x%p]"), this));
  ASSERT1(current_state);
  UNREFERENCED_PARAMETER(current_state);
  ASSERT(false, (_T("Not implemented. Should not call at this time.")));
  return E_NOTIMPL;
}

// This function is only called internal to the COM server and affects a
// separate vector of Apps, so it can be called in any state.
// It assumes all calls have a unique app_id.
HRESULT AppBundle::CreateUninstalledApp(const CString& app_id, App** app) {
  CORE_LOG(L1, (_T("[AppBundle::CreateUninstalledApp][%s][0x%p]"),
                app_id, this));
  ASSERT1(app);

  __mutexScope(model()->lock());

  GUID app_guid = {0};
  HRESULT hr = StringToGuidSafe(app_id, &app_guid);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[invalid app id][%s]"), app_id));
    return hr;
  }

  std::unique_ptr<App> local_app(new App(app_guid, true, this));

  hr = AppManager::Instance()->ReadUninstalledAppPersistentData(
           local_app.get());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ReadUninstalledAppPersistentData failed][0x%x][%s]"),
                  hr, app_id));
    return hr;
  }

  uninstalled_apps_.push_back(local_app.get());

  *app = local_app.release();
  return S_OK;
}

void AppBundle::CompleteAsyncCall() {
  __mutexScope(model()->lock());

  ASSERT1(is_pending_non_blocking_call());

  VERIFY_SUCCEEDED(app_bundle_state_->CompleteAsyncCall(this));

  user_work_item_ = NULL;
}

bool AppBundle::IsBusy() const {
  __mutexScope(model()->lock());
  const bool is_busy = app_bundle_state_->IsBusy();
  CORE_LOG(L3, (_T("[AppBundle::isBusy returned][0x%p][%u]"), this, is_busy));
  return is_busy;
}

void AppBundle::ChangeState(fsm::AppBundleState* app_bundle_state) {
  ASSERT1(app_bundle_state);
  ASSERT1(model()->IsLockedByCaller());

  app_bundle_state_.reset(app_bundle_state);
}


//
// AppBundleWrapper implementation.
//

AppBundleWrapper::AppBundleWrapper() {
  CORE_LOG(L3, (_T("[AppBundleWrapper::AppBundleWrapper][0x%p]"), this));
}

AppBundleWrapper::~AppBundleWrapper() {
  CORE_LOG(L3, (_T("[AppBundleWrapper::~AppBundleWrapper][0x%p]"), this));
}

//
// IAppBundle.
//

STDMETHODIMP AppBundleWrapper::get_displayName(BSTR* display_name) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_displayName(display_name);
}

STDMETHODIMP AppBundleWrapper::put_displayName(BSTR display_name) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_displayName(display_name);
}

STDMETHODIMP AppBundleWrapper::get_installSource(BSTR* install_source) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_installSource(install_source);
}

STDMETHODIMP AppBundleWrapper::put_installSource(BSTR install_source) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_installSource(install_source);
}

STDMETHODIMP AppBundleWrapper::get_originURL(BSTR* origin_url) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_originURL(origin_url);
}

STDMETHODIMP AppBundleWrapper::put_originURL(BSTR origin_url) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_originURL(origin_url);
}

STDMETHODIMP AppBundleWrapper::get_offlineDirectory(BSTR* offline_dir) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_offlineDirectory(offline_dir);
}

STDMETHODIMP AppBundleWrapper::put_offlineDirectory(BSTR offline_dir) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_offlineDirectory(offline_dir);
}

STDMETHODIMP AppBundleWrapper::get_sessionId(BSTR* session_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_sessionId(session_id);
}

STDMETHODIMP AppBundleWrapper::put_sessionId(BSTR session_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_sessionId(session_id);
}

STDMETHODIMP AppBundleWrapper::get_sendPings(VARIANT_BOOL* send_pings) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_sendPings(send_pings);
}

STDMETHODIMP AppBundleWrapper::put_sendPings(VARIANT_BOOL send_pings) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_sendPings(send_pings);
}

STDMETHODIMP AppBundleWrapper::get_priority(long* priority) {  // NOLINT
  __mutexScope(model()->lock());
  return wrapped_obj()->get_priority(priority);
}

STDMETHODIMP AppBundleWrapper::put_priority(long priority) {  // NOLINT
  __mutexScope(model()->lock());
  return wrapped_obj()->put_priority(priority);
}

STDMETHODIMP AppBundleWrapper::put_altTokens(ULONG_PTR impersonation_token,
                                             ULONG_PTR primary_token,
                                             DWORD caller_proc_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_altTokens(impersonation_token,
                                      primary_token,
                                      caller_proc_id);
}

STDMETHODIMP AppBundleWrapper::put_parentHWND(ULONG_PTR hwnd) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_parentHWND(hwnd);
}

STDMETHODIMP AppBundleWrapper::get_displayLanguage(BSTR* language) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_displayLanguage(language);
}
STDMETHODIMP AppBundleWrapper::put_displayLanguage(BSTR language) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_displayLanguage(language);
}

STDMETHODIMP AppBundleWrapper::initialize() {
  __mutexScope(model()->lock());
  return wrapped_obj()->initialize();
}

STDMETHODIMP AppBundleWrapper::createApp(BSTR app_id, IDispatch** app_disp) {
  __mutexScope(model()->lock());

  App* app = NULL;
  HRESULT hr = wrapped_obj()->createApp(app_id, &app);
  if (FAILED(hr)) {
    return hr;
  }

  return AppWrapper::Create(controlling_ptr(), app, app_disp);
}

STDMETHODIMP AppBundleWrapper::createInstalledApp(BSTR appId,
                                                  IDispatch** app_disp) {
  __mutexScope(model()->lock());

  App* app = NULL;
  HRESULT hr = wrapped_obj()->createInstalledApp(appId, &app);
  if (FAILED(hr)) {
    return hr;
  }

  return AppWrapper::Create(controlling_ptr(), app, app_disp);
}

STDMETHODIMP AppBundleWrapper::createAllInstalledApps() {
  __mutexScope(model()->lock());
  return wrapped_obj()->createAllInstalledApps();
}

STDMETHODIMP AppBundleWrapper::get_Count(long* count) {  // NOLINT
  __mutexScope(model()->lock());
  return wrapped_obj()->get_Count(count);
}

STDMETHODIMP AppBundleWrapper::get_Item(long index, IDispatch** app_disp) {  // NOLINT
  __mutexScope(model()->lock());

  App* app = NULL;
  HRESULT hr = wrapped_obj()->get_Item(index, &app);
  if (FAILED(hr)) {
    return hr;
  }

  return AppWrapper::Create(controlling_ptr(), app, app_disp);
}

STDMETHODIMP AppBundleWrapper::checkForUpdate() {
  __mutexScope(model()->lock());
  return wrapped_obj()->checkForUpdate();
}

STDMETHODIMP AppBundleWrapper::download() {
  __mutexScope(model()->lock());
  return wrapped_obj()->download();
}

STDMETHODIMP AppBundleWrapper::install() {
  if (wrapped_obj()->is_machine() && !UserRights::VerifyCallerIsAdmin()) {
    ASSERT(false, (_T("AppBundle::install - Caller not an admin")));
    return E_ACCESSDENIED;
  }

  __mutexScope(model()->lock());
  return wrapped_obj()->install();
}

STDMETHODIMP AppBundleWrapper::updateAllApps() {
  __mutexScope(model()->lock());
  return wrapped_obj()->updateAllApps();
}

STDMETHODIMP AppBundleWrapper::stop() {
  __mutexScope(model()->lock());
  return wrapped_obj()->stop();
}

STDMETHODIMP AppBundleWrapper::pause() {
  __mutexScope(model()->lock());
  return wrapped_obj()->pause();
}

STDMETHODIMP AppBundleWrapper::resume() {
  __mutexScope(model()->lock());
  return wrapped_obj()->resume();
}

STDMETHODIMP AppBundleWrapper::isBusy(VARIANT_BOOL* is_busy) {
  __mutexScope(model()->lock());
  return wrapped_obj()->isBusy(is_busy);
}

STDMETHODIMP AppBundleWrapper::downloadPackage(BSTR app_id, BSTR package_name) {
  __mutexScope(model()->lock());
  return wrapped_obj()->downloadPackage(app_id, package_name);
}

STDMETHODIMP AppBundleWrapper::get_currentState(VARIANT* current_state) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_currentState(current_state);
}


// Sets app bundle's app_state to state. Used by unit tests to set up the state
// to the correct precondition for the test case. AppBundle friends this
// function, allowing it to call the private member function.
void SetAppBundleStateForUnitTest(AppBundle* app_bundle,
                                  fsm::AppBundleState* state) {
  ASSERT1(app_bundle);
  ASSERT1(state);
  __mutexScope(app_bundle->model()->lock());
  app_bundle->ChangeState(state);
}

}  // namespace omaha
