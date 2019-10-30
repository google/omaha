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

// Defines the AppBundle COM object exposed by the model.

#ifndef OMAHA_GOOPDATE_APP_BUNDLE_H_
#define OMAHA_GOOPDATE_APP_BUNDLE_H_

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/synchronized.h"
#include "omaha/common/ping.h"
#include "omaha/goopdate/com_wrapper_creator.h"
#include "omaha/goopdate/model_object.h"
#include "omaha/net/proxy_auth.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

// TODO(omaha): needs to figure out a smaller public interface that
// Worker expose for the model. AppBundle needs to delegate calls to Worker,
// such as Pause, Resume, Update, Install, etc...

class App;
class Model;
class WebServicesClientInterface;
class UserWorkItem;

namespace fsm {

class AppBundleState;
class AppBundleStateInit;

}  // namespace fsm

namespace internal {

struct SendPingEventsParameters {
 public:
  SendPingEventsParameters(Ping* p, HANDLE token)
      : ping(p),
        impersonation_token(token) {
    ASSERT1(ping);
  }

  Ping* ping;
  HANDLE impersonation_token;
};

}  // namespace internal

// AppBundle instances are reference-counted using shared pointers. Lifetime
// of an AppBundle instance is controlled by both external and internal
// outstanding references. External reference are the COM wrappers that depend
// on the AppBundle, including its children in the object model.
// Internal references to the AppBundle are maintained by several objects that
// depend on the bundle objects.
class AppBundle
    : public ModelObject,
      public std::enable_shared_from_this<AppBundle> {
 public:
  AppBundle(bool is_machine, Model* model);
  virtual ~AppBundle();

  // IAppBundle.
  STDMETHOD(get_displayName)(BSTR* display_name);
  STDMETHOD(put_displayName)(BSTR display_name);
  STDMETHOD(get_displayLanguage)(BSTR* language);
  STDMETHOD(put_displayLanguage)(BSTR language);
  STDMETHOD(get_installSource)(BSTR* install_source);
  STDMETHOD(put_installSource)(BSTR install_source);
  STDMETHOD(get_originURL)(BSTR* origin_url);
  STDMETHOD(put_originURL)(BSTR origin_url);
  STDMETHOD(get_offlineDirectory)(BSTR* offline_dir);
  STDMETHOD(put_offlineDirectory)(BSTR offline_dir);
  STDMETHOD(get_sessionId)(BSTR* session_id);
  STDMETHOD(put_sessionId)(BSTR session_id);
  STDMETHOD(get_sendPings)(VARIANT_BOOL* send_pings);
  STDMETHOD(put_sendPings)(VARIANT_BOOL send_pings);
  STDMETHOD(get_priority)(long* priority);  // NOLINT
  STDMETHOD(put_priority)(long priority);  // NOLINT
  STDMETHOD(get_Count)(long* count);  // NOLINT
  STDMETHOD(get_Item)(long index, App** app);  // NOLINT
  STDMETHOD(put_altTokens)(ULONG_PTR impersonation_token,
                           ULONG_PTR primary_token,
                           DWORD caller_proc_id);
  STDMETHOD(put_parentHWND)(ULONG_PTR hwnd);
  STDMETHOD(initialize)();
  STDMETHOD(createApp)(BSTR app_id, App** app);
  STDMETHOD(createInstalledApp)(BSTR app_id, App** app);
  STDMETHOD(createAllInstalledApps)();
  STDMETHOD(checkForUpdate)();
  STDMETHOD(download)();
  STDMETHOD(install)();
  STDMETHOD(updateAllApps)();
  STDMETHOD(stop)();
  STDMETHOD(pause)();
  STDMETHOD(resume)();
  STDMETHOD(isBusy)(VARIANT_BOOL* is_busy);
  STDMETHOD(downloadPackage)(BSTR app_id, BSTR package_name);
  STDMETHOD(get_currentState)(VARIANT* current_state);

  // Creates an App for each uninstalled app and adds it to
  HRESULT CreateUninstalledApp(const CString& app_id, App** app);

  // Marks an asynchronous operation complete.
  void CompleteAsyncCall();

  bool IsBusy() const;

  // Returns a shared pointer to this instance of the class under the
  // assumption that the instance is already managed by a shared pointer.
  // This shared pointer controls the lifetime of the app bundle object and
  // all its children.
  ControllingPtr controlling_ptr();

  void set_user_work_item(UserWorkItem* user_work_item);

  const CString& install_source() const { return install_source_; }

  const CString& origin_url() const { return origin_url_; }

  // Gets the impersonation token of the current COM caller.
  HANDLE impersonation_token() const;

  // Gets the primary token of the current COM caller.
  HANDLE primary_token() const;

  size_t GetNumberOfApps() const;

  App* GetApp(size_t index);

  WebServicesClientInterface* update_check_client();

  bool is_machine() const;

  bool is_auto_update() const;
  void set_is_auto_update(bool is_auto_update);

  bool is_offline_install() const;

  const CString& offline_dir() const;

  const CString& session_id() const;

  CString display_language() const;

  int priority() const;

  ProxyAuthConfig GetProxyAuthConfig() const;

  // Gathers accumulated event logs from all child apps and clears the
  // log buffer in each app.
  CString FetchAndResetLogText();

  // Builds a new Ping with all the Apps in the AppBundle and persists the Ping
  // in the registry.
  HRESULT BuildAndPersistPing();

 private:
  // Sets the state for unit testing.
  friend void SetAppBundleStateForUnitTest(AppBundle* app_bundle,
                                           fsm::AppBundleState* state);

  // Builds a new Ping object with all the Apps in the AppBundle.
  void BuildPing(std::unique_ptr<Ping>* ping);

  // TODO(omaha): missing unit test.
  // Sends the ping if the applications in the bundle have accumulated
  // any ping events.
  HRESULT SendPingEventsAsync();
  static void SendPingEvents(internal::SendPingEventsParameters params);

  // These methods capture the current COM caller tokens.
  HRESULT CaptureCallerImpersonationToken();
  HRESULT CaptureCallerPrimaryToken();

  void ChangeState(fsm::AppBundleState* app_bundle_state);

  bool is_pending_non_blocking_call() const;

  CString display_name_;
  CString install_source_;
  CString origin_url_;

  bool is_machine_;

  // True if the bundle is an update bundle.
  bool is_auto_update_;

  // If false, omit sending event pings when the bundle is destroyed.
  bool send_pings_;

  int priority_;

  HWND parent_hwnd_;

  CString offline_dir_;

  // Contains the session ID - a unique marker that we include with each
  // server communication (update checks, pings, etc.) in a single Omaha task.
  // Clients are expected to set this on a bundle before calling initialize();
  // if they don't, we will randomly generate one.
  CString session_id_;

  // The request id is unique for the Ping requests sent to the Omaha server for
  // this AppBundle. Persisted Pings for this AppBundle are also stored in the
  // registry under this unique key.
  CString request_id_;

  // The current non-blocking command object if any of them is executing.
  // The class only checks whether the pointer is NULL to determine if a
  // non-blocking call is pending. We use a pointer because it can be useful
  // for debugging. The object is not owned by this class.
  UserWorkItem* user_work_item_;

  std::unique_ptr<WebServicesClientInterface> update_check_client_;

  // The apps in the bundle. Do not add to it directly; use AddApp() instead.
  std::vector<App*> apps_;

  // Uninstalled apps. Not accessible and only used to store Apps for
  // uninstalled app IDs so that app uninstall pings can be sent along with
  // other pings.
  std::vector<App*> uninstalled_apps_;

  std::unique_ptr<fsm::AppBundleState> app_bundle_state_;

  // Impersonation and primary tokens set by the client. Typically only
  // set by the gupdatem service. The gupdatem service exposes a narrow
  // interface to medium integrity clients. When a medium integrity client calls
  // into the gupdatem service, the gupdatem service captures the token of the
  // caller, and then calls put_altTokens() on the gupdate service, so that the
  // gupdate service can use it for future download() and install() requests.
  CAccessToken alt_impersonation_token_;
  CAccessToken alt_primary_token_;

  // The current COM caller's impersonation token.
  CAccessToken impersonation_token_;

  // The current COM caller's primary token. Lazy initialized at the install()
  // entry point.
  CAccessToken primary_token_;

  // COM caller's display language.
  CString display_language_;

  friend class fsm::AppBundleState;
  friend class fsm::AppBundleStateInit;

  friend class AppBundleTest;
  friend class PersistedPingsTest;
  friend class WorkerTest;

  DISALLOW_COPY_AND_ASSIGN(AppBundle);
};

class ATL_NO_VTABLE AppBundleWrapper
    : public ComWrapper<AppBundleWrapper, AppBundle>,
      public IDispatchImpl<IAppBundle,
                           &__uuidof(IAppBundle),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  AppBundleWrapper();
  virtual ~AppBundleWrapper();

  // IAppBundle.
  STDMETHOD(get_displayName)(BSTR* display_name);
  STDMETHOD(put_displayName)(BSTR display_name);
  STDMETHOD(get_displayLanguage)(BSTR* language);
  STDMETHOD(put_displayLanguage)(BSTR language);
  STDMETHOD(get_installSource)(BSTR* install_source);
  STDMETHOD(put_installSource)(BSTR install_source);
  STDMETHOD(get_originURL)(BSTR* origin_url);
  STDMETHOD(put_originURL)(BSTR origin_url);
  STDMETHOD(get_offlineDirectory)(BSTR* offline_dir);
  STDMETHOD(put_offlineDirectory)(BSTR offline_dir);
  STDMETHOD(get_sessionId)(BSTR* session_id);
  STDMETHOD(put_sessionId)(BSTR session_id);
  STDMETHOD(get_sendPings)(VARIANT_BOOL* send_pings);
  STDMETHOD(put_sendPings)(VARIANT_BOOL send_pings);
  STDMETHOD(get_priority)(long* priority);  // NOLINT
  STDMETHOD(put_priority)(long priority);  // NOLINT
  STDMETHOD(get_Count)(long* count);  // NOLINT
  STDMETHOD(get_Item)(long index, IDispatch** app_disp);  // NOLINT
  STDMETHOD(put_altTokens)(ULONG_PTR impersonation_token,
                           ULONG_PTR primary_token,
                           DWORD caller_proc_id);
  STDMETHOD(put_parentHWND)(ULONG_PTR hwnd);
  STDMETHOD(initialize)();
  STDMETHOD(createApp)(BSTR app_id, IDispatch** app_disp);
  STDMETHOD(createInstalledApp)(BSTR app_id, IDispatch** app_disp);
  STDMETHOD(createAllInstalledApps)();
  STDMETHOD(checkForUpdate)();
  STDMETHOD(download)();
  STDMETHOD(install)();
  STDMETHOD(updateAllApps)();
  STDMETHOD(stop)();
  STDMETHOD(pause)();
  STDMETHOD(resume)();
  STDMETHOD(isBusy)(VARIANT_BOOL* is_busy);
  STDMETHOD(downloadPackage)(BSTR app_id, BSTR package_name);
  STDMETHOD(get_currentState)(VARIANT* current_state);

 private:
  BEGIN_COM_MAP(AppBundleWrapper)
    COM_INTERFACE_ENTRY(IAppBundle)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_BUNDLE_H_
