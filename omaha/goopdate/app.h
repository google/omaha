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

// Defines the App COM object exposed by the model. App tracks two versions:
//  - the version currently installed
//  - the future version to be updated to, if such a version exists

#ifndef OMAHA_GOOPDATE_APP_H_
#define OMAHA_GOOPDATE_APP_H_

#include <atlbase.h>
#include <atlcom.h>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/constants.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/ping_event.h"
#include "omaha/common/protocol_definition.h"
#include "omaha/goopdate/com_wrapper_creator.h"
#include "omaha/goopdate/installer_result_info.h"
#include "omaha/goopdate/model_object.h"

namespace omaha {

// Stores the error codes associated with a particular error.
struct ErrorContext {
  ErrorContext() : error_code(S_OK), extra_code1(0) {}
  explicit ErrorContext(HRESULT hr, int code1 = 0)
      : error_code(hr),
        extra_code1(code1) {}

  HRESULT error_code;
  int     extra_code1;

  // Add more extra codes here as needed.
};

class DownloadManagerInterface;
class InstallManagerInterface;

namespace fsm {

class AppState;

}  // namespace fsm

namespace xml {

class UpdateRequest;
class UpdateResponse;

}  // namespace xml

class AppBundle;
class AppCommandModel;
class AppVersion;
class CurrentAppState;

class App : public ModelObject {
 public:
  App(const GUID& app_guid, bool is_update, AppBundle* app_bundle);
  virtual ~App();

  AppVersion* current_version();
  const AppVersion* current_version() const;

  AppVersion* next_version();
  const AppVersion* next_version() const;

  AppCommandModel* command(const CString& command_id);
  const AppCommandModel* command(const CString& command_id) const;

  AppBundle* app_bundle();
  const AppBundle* app_bundle() const;

  CString app_guid_string() const;

  // TODO(omaha): refactor so that the app guid setter is only used by tests.
  GUID app_guid() const;
  void set_app_guid(const GUID& app_guid);

  CString language() const;

  bool is_eula_accepted() const;

  CString display_name() const;

  CurrentState state() const;

  bool is_install() const { return !is_update(); }
  bool is_update() const;

  // Whether this app is bundled together with other apps.
  bool is_bundled() const;

  bool has_update_available() const;
  void set_has_update_available(bool has_update_available);

  GUID iid() const;

  CString client_id() const;

  CString GetExperimentLabels() const;

  CString GetExperimentLabelsNoTimestamps() const;

  CString referral_id() const;

  BrowserType browser_type() const;

  Tristate usage_stats_enable() const;

  CString client_install_data() const;

  CString server_install_data() const;
  void set_server_install_data(const CString& server_install_data);

  CString brand_code() const;

  uint32 install_time_diff_sec() const;

  int day_of_install() const;

  ActiveStates did_run() const;

  int days_since_last_active_ping() const;
  void set_days_since_last_active_ping(int days);

  int days_since_last_roll_call() const;
  void set_days_since_last_roll_call(int days);

  int day_of_last_activity() const;
  void set_day_of_last_activity(int day_num);

  int day_of_last_roll_call() const;
  void set_day_of_last_roll_call(int day_num);

  int day_of_last_response() const;
  void set_day_of_last_response(int day_num);

  CString ping_freshness() const;

  CString ap() const;

  std::vector<StringPair> app_defined_attributes() const;

  CString tt_token() const;

  Cohort cohort() const;
  void set_cohort(const Cohort& cohort);

  CString server_install_data_index() const;

  CString untrusted_data() const;

  HRESULT error_code() const;

  ErrorContext error_context() const;

  int installer_result_code() const;

  int installer_result_extra_code1() const;

  const PingEventVector& ping_events() const;

  AppVersion* working_version();
  const AppVersion* working_version() const;

  bool can_skip_signature_verification() const;
  void set_can_skip_signature_verification(
      bool can_skip_signature_verification);

  void set_external_updater_event(HANDLE event_handle);

  int source_url_index() const;

  void set_source_url_index(int index);

  CurrentState state_cancelled() const;

  void set_state_cancelled(CurrentState state_cancelled);

  // IApp.
  STDMETHOD(get_appId)(BSTR* app_id);

  STDMETHOD(get_pv)(BSTR* pv);
  STDMETHOD(put_pv)(BSTR pv);

  STDMETHOD(get_language)(BSTR* language);
  STDMETHOD(put_language)(BSTR language);

  STDMETHOD(get_ap)(BSTR* ap);
  STDMETHOD(put_ap)(BSTR ap);

  STDMETHOD(get_ttToken)(BSTR* tt_token);
  STDMETHOD(put_ttToken)(BSTR tt_token);

  STDMETHOD(get_iid)(BSTR* iid);
  STDMETHOD(put_iid)(BSTR iid);

  STDMETHOD(get_brandCode)(BSTR* brand_code);
  STDMETHOD(put_brandCode)(BSTR brand_code);

  STDMETHOD(get_clientId)(BSTR* client_id);
  STDMETHOD(put_clientId)(BSTR client_id);

  STDMETHOD(get_labels)(BSTR* labels);
  STDMETHOD(put_labels)(BSTR labels);

  STDMETHOD(get_referralId)(BSTR* referral_id);
  STDMETHOD(put_referralId)(BSTR referral_id);

  STDMETHOD(get_installTimeDiffSec)(UINT* install_time_diff_sec);

  STDMETHOD(get_isEulaAccepted)(VARIANT_BOOL* is_eula_accepted);
  STDMETHOD(put_isEulaAccepted)(VARIANT_BOOL is_eula_accepted);

  STDMETHOD(get_displayName)(BSTR* display_name);
  STDMETHOD(put_displayName)(BSTR display_name);

  STDMETHOD(get_browserType)(UINT* browser_type);
  STDMETHOD(put_browserType)(UINT browser_type);

  STDMETHOD(get_clientInstallData)(BSTR* data);
  STDMETHOD(put_clientInstallData)(BSTR data);

  STDMETHOD(get_serverInstallDataIndex)(BSTR* index);
  STDMETHOD(put_serverInstallDataIndex)(BSTR index);

  STDMETHOD(get_usageStatsEnable)(UINT* usage_stats_enable);
  STDMETHOD(put_usageStatsEnable)(UINT usage_stats_enable);

  STDMETHOD(get_currentState)(IDispatch** current_state);

  // IApp2.
  STDMETHOD(get_untrustedData)(BSTR* index);
  STDMETHOD(put_untrustedData)(BSTR index);

  // Sets the error context and the completion message.
  void SetNoUpdate(const ErrorContext& error_context, const CString& message);
  void SetError(const ErrorContext& error_context, const CString& message);

  // Records the details of the installer success or failure.
  void SetInstallerResult(const InstallerResultInfo& result_info);

  // Gets the appropriate installer data regardless of the source. Only valid
  // in STATE_UPDATE_AVAILABLE and later.
  CString GetInstallData() const;


  //
  // App state machine transition conditions. These functions make
  // the application change states.
  //

  // Sets the app to Waiting To Check.
  void QueueUpdateCheck();

  // Adds the app to the update request.
  void PreUpdateCheck(xml::UpdateRequest* update_request);

  // Processes the update response for the app.
  void PostUpdateCheck(HRESULT result, xml::UpdateResponse* update_response);

  // Sets the app to Waiting To Download.
  void QueueDownload();

  // Sets the app to Waiting To Download or Waiting To Install depending on the
  // current state.
  void QueueDownloadOrInstall();

  // Initiates download of the app if necessary.
  void Download(DownloadManagerInterface* download_manager);

  // Reports that the download is in progress. May be called multiple times.
  void Downloading();

  // Reports that all packages have been downloaded.
  void DownloadComplete();

  // Sets the app to Ready To install.
  void MarkReadyToInstall();

  // Sets the app to Waiting To Install.
  void QueueInstall();

  // Initiates installation of the app.
  void Install(InstallManagerInterface* install_manager);

  // Reports that the install is in progress. May be called multiple times.
  void Installing();

  // Reports that the app installer has completed. Can be success or failure.
  void ReportInstallerComplete(const InstallerResultInfo& result_info);

  // TODO(omaha3): What does this pause?
  void Pause();

  // Cancels the app install.
  void Cancel();

  // Stops installation in the Error state.
  void Error(const ErrorContext& error_context, const CString& message);

  // For logging support.
  CString FetchAndResetLogText();
  void LogTextAppendFormat(const TCHAR* format, ...);

  // Adds an event to the app's ping, which is sent when the bundle is
  // destroyed.
  void AddPingEvent(const PingEventPtr& ping_event);

  // Returns an error if update/install, as determined by is_update_, is
  // disabled by Group Policy.
  HRESULT CheckGroupPolicy() const;

  // Returns the target channel for the app, if the machine is joined to a
  // domain and has the corresponding policy set.
  CString GetTargetChannel() const;

  // Returns whether the RollbackToTargetVersion policy has been set for the
  // app.
  bool IsRollbackToTargetVersionAllowed() const;

  // Returns the target version prefix for the app, if the machine is joined to
  // a domain and has the corresponding group policy set.
  CString GetTargetVersionPrefix() const;

  // Updates num bytes downloaded by adding newly downloaded bytes.
  void UpdateNumBytesDownloaded(uint64 num_bytes);

  // Returns how many bytes are actually downloaded.
  uint64 num_bytes_downloaded() const;

  // Returns the size sum of all packages for this app.
  uint64 GetPackagesTotalSize() const;

  enum TimeMetricType {
    TIME_UPDATE_AVAILABLE = 0,
    TIME_DOWNLOAD_START = 1,
    TIME_DOWNLOAD_COMPLETE = 2,
    TIME_INSTALL_START = 3,
    TIME_INSTALL_COMPLETE = 4,
    TIME_UPDATE_CHECK_START = 5,
    TIME_UPDATE_CHECK_COMPLETE = 6,
    TIME_CANCELLED = 7,

    // Add new metrics above this line.
    TIME_METRICS_MAX
  };

  // Sets current time as specified time.
  void SetCurrentTimeAs(TimeMetricType time_type);

  // Returns how long it takes for the download manager to download this app.
  int GetDownloadTimeMs() const;

  // Returns how long it takes for the install manager to install this app.
  int GetInstallTimeMs() const;

  // Returns how long it takes to do the update check.
  int GetUpdateCheckTimeMs() const;

  // Returns the time interval between update is available and user cancel.
  int GetTimeSinceUpdateAvailable() const;

  // Returns the time interval between update is available and user cancel.
  int GetTimeSinceDownloadStart() const;

  // Deletes "InstallerProgress" under Google\\Update\\ClientState\\{AppID}.
  HRESULT ResetInstallProgress();

 private:
  // TODO(omaha): accessing directly the data members bypasses locking. Review
  // the places where members are accessed by friends and check the caller locks
  // before going directly for the private members.
  friend class AppManager;
  friend class AppManagerTestBase;

  // TODO(omaha3): Maybe use a mock in these tests instead.
  friend class InstallManagerInstallAppTest;

  friend class fsm::AppState;

  // Sets the app state for unit testing.
  friend void SetAppStateForUnitTest(App* app, fsm::AppState* state);

  HRESULT GetDownloadProgress(uint64* bytes_downloaded,
                              uint64* bytes_total,
                              LONG* time_remaining_ms,
                              uint64* next_retry_time);
  HRESULT GetInstallProgress(LONG* install_progress_percentage,
                             LONG* install_time_remaining_ms);

  void ChangeState(fsm::AppState* app_state);

  int GetTimeDifferenceMs(TimeMetricType time_start_metric_type,
                          TimeMetricType time_end_metric_type) const;

  std::unique_ptr<fsm::AppState> app_state_;

  std::unique_ptr<AppVersion> current_version_;
  std::unique_ptr<AppVersion> next_version_;

  // Alias to the version of the app that is being modified.
  AppVersion* working_version_;

  PingEventVector ping_events_;

  CString event_log_text_;

  // Weak reference to the containing bundle.
  AppBundle* app_bundle_;

  // True if the app is in the update scenario.
  const bool is_update_;

  // True if the server responded that an update is available for the app.
  // This can happen in both install and update cases.
  bool has_update_available_;

  GUID app_guid_;
  CString pv_;

  // These values are stored in Clients. language can be in ClientState too.
  CString language_;
  CString display_name_;

  // These values are stored in ClientState.
  std::vector<StringPair> app_defined_attributes_;
  CString ap_;
  CString tt_token_;
  Cohort cohort_;
  GUID iid_;
  CString brand_code_;
  CString client_id_;
  // TODO(omaha3): Rename member and registry value to match the COM property.
  CString referral_id_;
  uint32 install_time_diff_sec_;
  Tristate is_eula_accepted_;
  BrowserType browser_type_;
  int days_since_last_active_ping_;
  int days_since_last_roll_call_;
  CString ping_freshness_;

  // The values are number of days since a particular datum when the event
  // happened. The datum is chosen by server side. They are from server's
  // response.
  int day_of_last_activity_;
  int day_of_last_roll_call_;
  int day_of_install_;
  int day_of_last_response_;

  // This value is stored in ClientState.
  Tristate usage_stats_enable_;

  // This value is stored by the clients in one of several registry locations.
  ActiveStates did_run_;

  // This value is not currently persisted in the registry.
  CString server_install_data_index_;

  // This value is not currently persisted in the registry.
  CString untrusted_data_;

  // Contains the installer data string specified by the client (usually
  // contained in /appdata).
  CString client_install_data_;

  // Contains the installer data string returned by the server for
  // server_install_data_index_.
  CString server_install_data_;

  bool is_canceled_;
  ErrorContext error_context_;
  CString completion_message_;
  PingEvent::Results completion_result_;
  int installer_result_code_;
  int installer_result_extra_code1_;
  CString post_install_launch_command_line_;
  CString post_install_url_;
  PostInstallAction post_install_action_;

  // Index of the URL that this app installer is downloaded from.
  int source_url_index_;

  // At which state is the app cancelled.
  CurrentState state_cancelled_;

  // We hold onto these just to be able to free them when the model is
  // destroyed. We don't actually return the same instance multiple times;
  // repeated queries for the same ID return distinct instances.
  std::vector<AppCommandModel*> loaded_app_commands_;

  // Handle to the global event used to suppress external updaters from running.
  // Should be released when this object is destroyed.
  scoped_event external_updater_event_;

  uint64 previous_total_download_bytes_;

  // Metrics values.
  uint64 num_bytes_downloaded_;
  uint64 time_metrics_[TIME_METRICS_MAX];

  // The packages of the app have been trusted by other means and the
  // signature verification can be omitted.
  bool can_skip_signature_verification_;

  DISALLOW_COPY_AND_ASSIGN(App);
};

class ATL_NO_VTABLE AppWrapper
    : public ComWrapper<AppWrapper, App>,
      public IDispatchImpl<IApp2,
                          &__uuidof(IApp2),
                          &CAtlModule::m_libid,
                          kMajorTypeLibVersion,
                          kMinorTypeLibVersion> {
 public:
  // IApp.
  STDMETHOD(get_currentVersion)(IDispatch** current);
  STDMETHOD(get_nextVersion)(IDispatch** next);
  STDMETHOD(get_command)(BSTR command_id, IDispatch** command);
  STDMETHOD(get_currentState)(IDispatch** current_state_disp);

  STDMETHOD(get_appId)(BSTR* app_id);

  STDMETHOD(get_pv)(BSTR* pv);
  STDMETHOD(put_pv)(BSTR pv);

  STDMETHOD(get_language)(BSTR* language);
  STDMETHOD(put_language)(BSTR language);

  STDMETHOD(get_ap)(BSTR* ap);
  STDMETHOD(put_ap)(BSTR ap);

  STDMETHOD(get_ttToken)(BSTR* tt_token);
  STDMETHOD(put_ttToken)(BSTR tt_token);

  STDMETHOD(get_iid)(BSTR* iid);
  STDMETHOD(put_iid)(BSTR iid);

  STDMETHOD(get_brandCode)(BSTR* brand_code);
  STDMETHOD(put_brandCode)(BSTR brand_code);

  STDMETHOD(get_clientId)(BSTR* client_id);
  STDMETHOD(put_clientId)(BSTR client_id);

  STDMETHOD(get_labels)(BSTR* labels);
  STDMETHOD(put_labels)(BSTR labels);

  STDMETHOD(get_referralId)(BSTR* referral_id);
  STDMETHOD(put_referralId)(BSTR referral_id);

  STDMETHOD(get_installTimeDiffSec)(UINT* install_time_diff_sec);

  STDMETHOD(get_isEulaAccepted)(VARIANT_BOOL* is_eula_accepted);
  STDMETHOD(put_isEulaAccepted)(VARIANT_BOOL is_eula_accepted);

  STDMETHOD(get_displayName)(BSTR* display_name);
  STDMETHOD(put_displayName)(BSTR display_name);

  STDMETHOD(get_browserType)(UINT* browser_type);
  STDMETHOD(put_browserType)(UINT browser_type);

  STDMETHOD(get_clientInstallData)(BSTR* data);
  STDMETHOD(put_clientInstallData)(BSTR data);

  STDMETHOD(get_serverInstallDataIndex)(BSTR* index);
  STDMETHOD(put_serverInstallDataIndex)(BSTR index);

  STDMETHOD(get_usageStatsEnable)(UINT* usage_stats_enable);
  STDMETHOD(put_usageStatsEnable)(UINT usage_stats_enable);

  // IApp2.
  STDMETHOD(get_untrustedData)(BSTR* index);
  STDMETHOD(put_untrustedData)(BSTR index);

 protected:
  AppWrapper() {}
  virtual ~AppWrapper() {}

  BEGIN_COM_MAP(AppWrapper)
    COM_INTERFACE_ENTRY(IApp2)
    COM_INTERFACE_ENTRY(IApp)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

 private:
  DISALLOW_COPY_AND_ASSIGN(AppWrapper);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_H_
