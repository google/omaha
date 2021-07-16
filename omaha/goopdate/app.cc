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

#include "omaha/goopdate/app.h"

#include "omaha/base/error.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/time.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/goopdate/app_command_model.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/app_state.h"
#include "omaha/goopdate/app_state_init.h"
#include "omaha/goopdate/current_state.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

App::App(const GUID& app_guid, bool is_update, AppBundle* app_bundle)
    : ModelObject(app_bundle->model()),
      app_bundle_(app_bundle),
      is_update_(is_update),
      has_update_available_(false),
      app_guid_(app_guid),
      iid_(GUID_NULL),
      install_time_diff_sec_(0),
      is_eula_accepted_(TRISTATE_NONE),  // Cannot ping until set true.
      browser_type_(BROWSER_UNKNOWN),
      days_since_last_active_ping_(0),
      days_since_last_roll_call_(0),
      day_of_last_activity_(0),
      day_of_last_roll_call_(0),
      day_of_install_(0),
      day_of_last_response_(0),
      usage_stats_enable_(TRISTATE_NONE),
      did_run_(ACTIVE_UNKNOWN),
      is_canceled_(false),
      completion_result_(PingEvent::EVENT_RESULT_SUCCESS),
      installer_result_code_(0),
      installer_result_extra_code1_(0),
      post_install_action_(POST_INSTALL_ACTION_DEFAULT),
      source_url_index_(-1),
      state_cancelled_(STATE_ERROR),
      previous_total_download_bytes_(0),
      num_bytes_downloaded_(0),
      can_skip_signature_verification_(false) {
  ASSERT1(!::IsEqualGUID(GUID_NULL, app_guid_));

  current_version_.reset(new AppVersion(this));
  next_version_.reset(new AppVersion(this));

  // TODO(omaha):  set the working_version_ correctly to indicate which
  // version of the app is modified: current version for components and
  // next version for install/updates. The code do not support components yet
  // and the working version in set to next always.
  working_version_ = next_version_.get();
  app_state_.reset(new fsm::AppStateInit);

  ::ZeroMemory(time_metrics_, sizeof(time_metrics_));
}

// Destruction of App objects happens within the scope of their parent,
// which controls the locking.
App::~App() {
  ASSERT1(model()->IsLockedByCaller());

  for (size_t i = 0; i < loaded_app_commands_.size(); ++i) {
    delete loaded_app_commands_[i];
  }

  working_version_ = NULL;
  app_bundle_ = NULL;
}

STDMETHODIMP App::get_appId(BSTR* app_id) {
  __mutexScope(model()->lock());
  ASSERT1(app_id);
  *app_id = GuidToString(app_guid_).AllocSysString();
  return S_OK;
}

STDMETHODIMP App::get_language(BSTR* language) {
  __mutexScope(model()->lock());
  ASSERT1(language);
  *language = language_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_language(BSTR language) {
  __mutexScope(model()->lock());
  language_ = language;
  return S_OK;
}

STDMETHODIMP App::get_ap(BSTR* ap) {
  __mutexScope(model()->lock());
  ASSERT1(ap);
  *ap = ap_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_ap(BSTR ap) {
  __mutexScope(model()->lock());
  ap_ = ap;
  return S_OK;
}

STDMETHODIMP App::get_pv(BSTR* pv) {
  __mutexScope(model()->lock());
  ASSERT1(pv);
  *pv = pv_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_pv(BSTR pv) {
  __mutexScope(model()->lock());
  pv_ = pv;
  return S_OK;
}

STDMETHODIMP App::get_ttToken(BSTR* tt_token) {
  __mutexScope(model()->lock());
  ASSERT1(tt_token);
  *tt_token = tt_token_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_ttToken(BSTR tt_token) {
  __mutexScope(model()->lock());
  tt_token_ = tt_token;
  return S_OK;
}

STDMETHODIMP App::get_iid(BSTR* iid) {
  __mutexScope(model()->lock());
  ASSERT1(iid);
  *iid = GuidToString(iid_).AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_iid(BSTR iid) {
  __mutexScope(model()->lock());
  return StringToGuidSafe(iid, &iid_);
}

STDMETHODIMP App::get_brandCode(BSTR* brand_code) {
  __mutexScope(model()->lock());
  ASSERT1(brand_code);
  *brand_code = brand_code_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_brandCode(BSTR brand_code) {
  __mutexScope(model()->lock());
  brand_code_ = brand_code;
  return S_OK;
}

STDMETHODIMP App::get_clientId(BSTR* client_id) {
  __mutexScope(model()->lock());
  ASSERT1(client_id);
  *client_id = client_id_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_clientId(BSTR client_id) {
  __mutexScope(model()->lock());
  client_id_ = client_id;
  return S_OK;
}

STDMETHODIMP App::get_labels(BSTR* labels) {
  __mutexScope(model()->lock());
  ASSERT1(labels);
  *labels = GetExperimentLabels().AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_labels(BSTR labels) {
  __mutexScope(model()->lock());
  return ExperimentLabels::WriteRegistry(app_bundle_->is_machine(),
                                         app_guid_string(),
                                         labels);
}

STDMETHODIMP App::get_referralId(BSTR* referral_id) {
  __mutexScope(model()->lock());
  ASSERT1(referral_id);
  *referral_id = referral_id_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_referralId(BSTR referral_id) {
  __mutexScope(model()->lock());
  referral_id_ = referral_id;
  return S_OK;
}

STDMETHODIMP App::get_installTimeDiffSec(UINT* install_time_diff_sec) {
  __mutexScope(model()->lock());
  ASSERT1(install_time_diff_sec);
  *install_time_diff_sec = install_time_diff_sec_;
  return S_OK;
}

STDMETHODIMP App::get_isEulaAccepted(VARIANT_BOOL* is_eula_accepted) {
  __mutexScope(model()->lock());
  ASSERT1(is_eula_accepted);
  *is_eula_accepted = App::is_eula_accepted() ? VARIANT_TRUE : VARIANT_FALSE;
  return S_OK;
}

STDMETHODIMP App::put_isEulaAccepted(VARIANT_BOOL is_eula_accepted) {
  __mutexScope(model()->lock());
  is_eula_accepted_ = is_eula_accepted ? TRISTATE_TRUE : TRISTATE_FALSE;
  return S_OK;
}

STDMETHODIMP App::get_displayName(BSTR* display_name) {
  __mutexScope(model()->lock());
  ASSERT1(display_name);
  *display_name = display_name_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_displayName(BSTR display_name) {
  __mutexScope(model()->lock());
  display_name_ = display_name;
  return S_OK;
}

STDMETHODIMP App::get_browserType(UINT* browser_type) {
  __mutexScope(model()->lock());
  ASSERT1(browser_type);
  *browser_type = browser_type_;
  return S_OK;
}

STDMETHODIMP App::put_browserType(UINT browser_type) {
  __mutexScope(model()->lock());
  if (browser_type >= BROWSER_MAX) {
    return E_INVALIDARG;
  }
  browser_type_ = static_cast<BrowserType>(browser_type);
  return S_OK;
}

STDMETHODIMP App::get_clientInstallData(BSTR* data) {
  __mutexScope(model()->lock());
  ASSERT1(data);
  *data = client_install_data_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_clientInstallData(BSTR data) {
  __mutexScope(model()->lock());
  client_install_data_ = data;
  return S_OK;
}

STDMETHODIMP App::get_serverInstallDataIndex(BSTR* index) {
  __mutexScope(model()->lock());
  ASSERT1(index);
  *index = server_install_data_index_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_serverInstallDataIndex(BSTR index) {
  __mutexScope(model()->lock());
  server_install_data_index_ = index;
  return S_OK;
}

STDMETHODIMP App::get_usageStatsEnable(UINT* usage_stats_enable) {
  __mutexScope(model()->lock());
  ASSERT1(usage_stats_enable);
  *usage_stats_enable = usage_stats_enable_;
  return S_OK;
}

STDMETHODIMP App::put_usageStatsEnable(UINT usage_stats_enable) {
  __mutexScope(model()->lock());
  if (usage_stats_enable > TRISTATE_NONE) {
    return E_INVALIDARG;
  }
  usage_stats_enable_ = static_cast<Tristate>(usage_stats_enable);
  return S_OK;
}

// TODO(omaha3): Replace decisions based on state() with calls to AppState.
// In this case, there should be a GetCurrentState() method on AppState.
STDMETHODIMP App::get_currentState(IDispatch** current_state) {
  __mutexScope(model()->lock());

  CORE_LOG(L6, (_T("[App::get_currentState][0x%p]"), this));
  ASSERT1(current_state);

  ULONGLONG bytes_downloaded = 0;
  ULONGLONG total_bytes_to_download = 0;
  ULONGLONG next_download_retry_time = 0;
  LONG download_time_remaining_ms = kCurrentStateProgressUnknown;
  LONG install_progress_percentage = kCurrentStateProgressUnknown;
  LONG install_time_remaining_ms = kCurrentStateProgressUnknown;

  HRESULT hr = S_OK;
  switch (state()) {
    case STATE_INIT:
      break;
    case STATE_WAITING_TO_CHECK_FOR_UPDATE:
      break;
    case STATE_CHECKING_FOR_UPDATE:
      break;
    case STATE_UPDATE_AVAILABLE:
      break;
    case STATE_NO_UPDATE:
      ASSERT1(error_code() == S_OK ||
              error_code() == GOOPDATE_E_UPDATE_DEFERRED);
      ASSERT1(!completion_message_.IsEmpty());
      ASSERT1(completion_result_ == PingEvent::EVENT_RESULT_SUCCESS ||
              completion_result_ == PingEvent::EVENT_RESULT_UPDATE_DEFERRED);
      ASSERT1(installer_result_code_ == 0);
      break;
    case STATE_WAITING_TO_DOWNLOAD:
    case STATE_RETRYING_DOWNLOAD:
    case STATE_DOWNLOADING:
    case STATE_DOWNLOAD_COMPLETE:
    case STATE_EXTRACTING:
    case STATE_APPLYING_DIFFERENTIAL_PATCH:
    case STATE_READY_TO_INSTALL:
      hr = GetDownloadProgress(&bytes_downloaded,
                               &total_bytes_to_download,
                               &download_time_remaining_ms,
                               &next_download_retry_time);
      if (SUCCEEDED(hr)) {
        VERIFY_SUCCEEDED(AppManager::Instance()->WriteDownloadProgress(
                *this,
                bytes_downloaded,
                total_bytes_to_download,
                download_time_remaining_ms));
      }
      break;
    case STATE_WAITING_TO_INSTALL:
      break;
    case STATE_INSTALLING:
      // Many installers do not write Installer Progress. We try to read it, but
      // we ignore any read errors.
      GetInstallProgress(&install_progress_percentage,
                         &install_time_remaining_ms);
      VERIFY_SUCCEEDED(AppManager::Instance()->WriteInstallProgress(
              *this, install_progress_percentage, install_time_remaining_ms));
      break;
    case STATE_INSTALL_COMPLETE:
      install_progress_percentage = 100;
      install_time_remaining_ms = 0;

      ASSERT1(error_code() == S_OK);
      ASSERT1(!completion_message_.IsEmpty());
      ASSERT1(completion_result_ == PingEvent::EVENT_RESULT_SUCCESS ||
              completion_result_ == PingEvent::EVENT_RESULT_SUCCESS_REBOOT);

      VERIFY_SUCCEEDED(AppManager::Instance()->WriteInstallProgress(
              *this, install_progress_percentage, install_time_remaining_ms));
      break;
    case STATE_PAUSED:
      break;
    case STATE_ERROR:
      ASSERT1(error_code() != S_OK);
      ASSERT1(!completion_message_.IsEmpty());
      ASSERT1(
          completion_result_ == PingEvent::EVENT_RESULT_ERROR ||
          completion_result_ == PingEvent::EVENT_RESULT_CANCELLED ||
          completion_result_ == PingEvent::EVENT_RESULT_INSTALLER_ERROR_MSI ||
          completion_result_ == PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER ||
          completion_result_ == PingEvent::EVENT_RESULT_INSTALLER_ERROR_SYSTEM);
      break;
    default:
      ASSERT1(false);
      hr = E_FAIL;
      break;
  }

  VERIFY_SUCCEEDED(AppManager::Instance()->WriteStateValue(*this, state()));

  if (FAILED(hr)) {
    return hr;
  }

  CComObject<CurrentAppState>* state_object = NULL;
  hr = CurrentAppState::Create(state(),
                               next_version()->version(),
                               bytes_downloaded,
                               total_bytes_to_download,
                               download_time_remaining_ms,
                               next_download_retry_time,
                               install_progress_percentage,
                               install_time_remaining_ms,
                               is_canceled_,
                               error_context_.error_code,
                               error_context_.extra_code1,
                               completion_message_,
                               installer_result_code_,
                               installer_result_extra_code1_,
                               post_install_launch_command_line_,
                               post_install_url_,
                               post_install_action_,
                               &state_object);
  if (FAILED(hr)) {
    return hr;
  }

  return state_object->QueryInterface(current_state);
}

STDMETHODIMP App::get_untrustedData(BSTR* data) {
  __mutexScope(model()->lock());
  ASSERT1(data);
  *data = untrusted_data_.AllocSysString();
  return S_OK;
}

STDMETHODIMP App::put_untrustedData(BSTR data) {
  __mutexScope(model()->lock());
  untrusted_data_ = data;
  return S_OK;
}

// TODO(omaha3): If some packages are already cached, there may be awkward jumps
// in progress if we don't filter those out of bytes_total from the beginning.
// TODO(omaha3): For now we use the package's expected_size to calculate
// bytes_total. This may or may not be what we want. See the TODO for
// Package::OnProgress().
// TODO(omaha3): Maybe optimize, especially in states other than
// STATE_DOWNLOADING.
HRESULT App::GetDownloadProgress(uint64* bytes_downloaded,
                                 uint64* bytes_total,
                                 LONG* time_remaining_ms,
                                 uint64* next_retry_time) {
  ASSERT1(model()->IsLockedByCaller());

  ASSERT1(bytes_downloaded);
  ASSERT1(bytes_total);
  ASSERT1(time_remaining_ms);
  ASSERT1(next_retry_time);

  *bytes_downloaded = 0;
  *bytes_total = 0;
  *next_retry_time = 0;

  *time_remaining_ms = kCurrentStateProgressUnknown;

  for (size_t i = 0; i < working_version_->GetNumberOfPackages(); ++i) {
    const Package* package = working_version_->GetPackage(i);

    const uint64 package_bytes = package->bytes_downloaded();
    ASSERT1((*bytes_downloaded + package_bytes > *bytes_downloaded) ||
            package_bytes == 0);
    *bytes_downloaded += package_bytes;

    const uint64 package_size = package->expected_size();
    ASSERT1(0 < package_size);
    ASSERT1(*bytes_total + package_size > *bytes_total);
    *bytes_total += package_size;

    LONG package_remaining_time_ms =
        package->GetEstimatedRemainingDownloadTimeMs();
    if (*time_remaining_ms < package_remaining_time_ms) {
      *time_remaining_ms = package_remaining_time_ms;
    }

    uint64 package_next_retry_time = package->next_download_retry_time();
    if (package_bytes < package_size && package_next_retry_time != 0 &&
        (*next_retry_time == 0 || *next_retry_time > package_next_retry_time)) {
      *next_retry_time = package_next_retry_time;
    }
  }

  ASSERT1(*bytes_downloaded <= *bytes_total);

  ASSERT1(previous_total_download_bytes_ == *bytes_total ||
          previous_total_download_bytes_ == 0);
  previous_total_download_bytes_ = *bytes_total;

  return S_OK;
}

HRESULT App::GetInstallProgress(LONG* install_progress_percentage,
                                LONG* install_time_remaining_ms) {
  ASSERT1(model()->IsLockedByCaller());

  ASSERT1(install_progress_percentage);
  ASSERT1(install_time_remaining_ms);

  *install_progress_percentage = kCurrentStateProgressUnknown;
  *install_time_remaining_ms = kCurrentStateProgressUnknown;

  // Installation progress is reported in "InstallerProgress" under
  // Google\\Update\\ClientState\\{AppID}. It is a value that goes from 0% to
  // 100%.
  const CString base_key_name(ConfigManager::Instance()->registry_client_state(
      app_bundle_->is_machine()));
  const CString app_id_key_name(AppendRegKeyPath(base_key_name,
                                                 app_guid_string()));
  DWORD progress_percent = 0;
  HRESULT hr = RegKey::GetValue(app_id_key_name,
                                kRegValueInstallerProgress,
                                &progress_percent);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[App::GetInstallProgress failed][%#x]"), hr));
    return hr;
  }

  *install_progress_percentage = std::min<DWORD>(100, progress_percent);

  CORE_LOG(L6, (_T("[App::GetInstallProgress][%s][%d][%d]"),
                app_guid_string(),
                progress_percent,
                *install_progress_percentage));
  return S_OK;
}

HRESULT App::ResetInstallProgress() {
  ASSERT1(model()->IsLockedByCaller());

  const CString base_key_name(ConfigManager::Instance()->registry_client_state(
      app_bundle_->is_machine()));
  const CString app_id_key_name(AppendRegKeyPath(base_key_name,
                                                 app_guid_string()));
  return RegKey::DeleteValue(app_id_key_name, kRegValueInstallerProgress);
}

AppBundle* App::app_bundle() {
  __mutexScope(model()->lock());
  return app_bundle_;
}

const AppBundle* App::app_bundle() const {
  __mutexScope(model()->lock());
  return app_bundle_;
}

AppVersion* App::current_version() {
  __mutexScope(model()->lock());
  return current_version_.get();
}

const AppVersion* App::current_version() const {
  __mutexScope(model()->lock());
  return current_version_.get();
}

AppVersion* App::next_version() {
  __mutexScope(model()->lock());
  return next_version_.get();
}

const AppVersion* App::next_version() const {
  __mutexScope(model()->lock());
  return next_version_.get();
}

AppCommandModel* App::command(const CString& command_id) {
  __mutexScope(model()->lock());
  AppCommandModel* command = NULL;
  HRESULT hr = AppCommandModel::Load(this,
                                     command_id,
                                     &command);
  if (FAILED(hr)) {
    // This includes undefined commands as well as system errors.
    // AppCommandModel::Load takes care of relevant logging.
    return NULL;
  }
  ASSERT1(command);

  // Make sure this command gets freed upon model destruction.
  loaded_app_commands_.push_back(command);

  return command;
}

CString App::app_guid_string() const {
  return GuidToString(app_guid());
}

GUID App::app_guid() const {
  __mutexScope(model()->lock());
  return app_guid_;
}

void App::set_app_guid(const GUID& app_guid) {
  __mutexScope(model()->lock());
  app_guid_ = app_guid;
}

CString App::language() const {
  __mutexScope(model()->lock());
  return language_;
}

bool App::is_eula_accepted() const {
  __mutexScope(model()->lock());
  return is_eula_accepted_ == TRISTATE_TRUE;
}

CString App::display_name() const {
  __mutexScope(model()->lock());
  return display_name_;
}

CurrentState App::state() const {
  __mutexScope(model()->lock());
  return app_state_->state();
}

bool App::is_update() const {
  __mutexScope(model()->lock());
  return is_update_;
}

bool App::is_bundled() const {
  __mutexScope(model()->lock());
  return app_bundle_->GetNumberOfApps() > 1;
}

bool App::has_update_available() const {
  __mutexScope(model()->lock());
  return has_update_available_;
}

void App::set_has_update_available(bool has_update_available) {
  __mutexScope(model()->lock());
  has_update_available_ = has_update_available;
}

GUID App::iid() const {
  __mutexScope(model()->lock());
  return iid_;
}

CString App::client_id() const {
  __mutexScope(model()->lock());
  return client_id_;
}

CString App::GetExperimentLabels() const {
  __mutexScope(model()->lock());
  return ExperimentLabels::ReadRegistry(app_bundle_->is_machine(),
                                        app_guid_string());
}

CString App::GetExperimentLabelsNoTimestamps() const {
  __mutexScope(model()->lock());
  return ExperimentLabels::RemoveTimestamps(GetExperimentLabels());
}

CString App::referral_id() const {
  __mutexScope(model()->lock());
  return referral_id_;
}

BrowserType App::browser_type() const {
  __mutexScope(model()->lock());
  return browser_type_;
}

Tristate App::usage_stats_enable() const {
  __mutexScope(model()->lock());
  return usage_stats_enable_;
}

CString App::client_install_data() const {
  __mutexScope(model()->lock());
  return client_install_data_;
}

CString App::server_install_data() const {
  __mutexScope(model()->lock());
  return server_install_data_;
}

void App::set_server_install_data(const CString& server_install_data) {
  __mutexScope(model()->lock());
  server_install_data_ = server_install_data;
}

CString App::brand_code() const {
  __mutexScope(model()->lock());
  return brand_code_;
}

// TODO(omaha): for better accuracy, compute the value when used.
uint32 App::install_time_diff_sec() const {
  __mutexScope(model()->lock());
  return install_time_diff_sec_;
}

int App::day_of_install() const {
  __mutexScope(model()->lock());
  return day_of_install_;
}

int App::day_of_last_response() const {
  __mutexScope(model()->lock());
  return day_of_last_response_;
}
void App::set_day_of_last_response(int day_num) {
  __mutexScope(model()->lock());
  day_of_last_response_ = day_num;
}

ActiveStates App::did_run() const {
  __mutexScope(model()->lock());
  return did_run_;
}

int App::days_since_last_active_ping() const {
  __mutexScope(model()->lock());
  return days_since_last_active_ping_;
}

void App::set_days_since_last_active_ping(int days) {
  __mutexScope(model()->lock());
  days_since_last_active_ping_ = days;
}

int App::days_since_last_roll_call() const {
  __mutexScope(model()->lock());
  return days_since_last_roll_call_;
}

void App::set_days_since_last_roll_call(int days) {
  __mutexScope(model()->lock());
  days_since_last_roll_call_ = days;
}

int App::day_of_last_activity() const {
  __mutexScope(model()->lock());
  return day_of_last_activity_;
}

void App::set_day_of_last_activity(int day_num) {
  __mutexScope(model()->lock());
  day_of_last_activity_ = day_num;
}

int App::day_of_last_roll_call() const {
  __mutexScope(model()->lock());
  return day_of_last_roll_call_;
}

void App::set_day_of_last_roll_call(int day_num) {
  __mutexScope(model()->lock());
  day_of_last_roll_call_ = day_num;
}

CString App::ping_freshness() const {
  __mutexScope(model()->lock());
  return ping_freshness_;
}

CString App::ap() const {
  __mutexScope(model()->lock());
  return ap_;
}

std::vector<StringPair> App::app_defined_attributes() const {
  __mutexScope(model()->lock());
  return app_defined_attributes_;
}

CString App::tt_token() const {
  __mutexScope(model()->lock());
  return tt_token_;
}

Cohort App::cohort() const {
  __mutexScope(model()->lock());
  return cohort_;
}

void App::set_cohort(const Cohort& cohort) {
  __mutexScope(model()->lock());
  cohort_ = cohort;
}

CString App::server_install_data_index() const {
  __mutexScope(model()->lock());
  return server_install_data_index_;
}

CString App::untrusted_data() const {
  __mutexScope(model()->lock());
  return untrusted_data_;
}

HRESULT App::error_code() const {
  __mutexScope(model()->lock());
  return error_context_.error_code;
}

ErrorContext App::error_context() const {
  __mutexScope(model()->lock());
  return error_context_;
}

int App::installer_result_code() const {
  __mutexScope(model()->lock());
  return installer_result_code_;
}

int App::installer_result_extra_code1() const {
  __mutexScope(model()->lock());
  return installer_result_extra_code1_;
}

const PingEventVector& App::ping_events() const {
  __mutexScope(model()->lock());
  return ping_events_;
}

AppVersion* App::working_version() {
  __mutexScope(model()->lock());
  return working_version_;
}

const AppVersion* App::working_version() const {
  __mutexScope(model()->lock());
  return working_version_;
}

bool App::can_skip_signature_verification() const {
  __mutexScope(model()->lock());
  return can_skip_signature_verification_;
}

void App::set_can_skip_signature_verification(
    bool can_skip_signature_verification) {
  __mutexScope(model()->lock());
  can_skip_signature_verification_ = can_skip_signature_verification;
}

void App::set_external_updater_event(HANDLE event_handle) {
  __mutexScope(model()->lock());
  reset(external_updater_event_, event_handle);
}

int App::source_url_index() const {
  __mutexScope(model()->lock());
  return source_url_index_;
}

void App::set_source_url_index(int index) {
  ASSERT1(index >= 0);
  __mutexScope(model()->lock());
  source_url_index_ = index;
}


CurrentState App::state_cancelled() const {
  __mutexScope(model()->lock());
  return state_cancelled_;
}

void App::set_state_cancelled(CurrentState state_cancelled) {
  __mutexScope(model()->lock());
  state_cancelled_ = state_cancelled;
}

CString App::FetchAndResetLogText() {
  __mutexScope(model()->lock());
  CString event_log_text(event_log_text_);
  event_log_text_.Empty();

  return event_log_text;
}

void App::LogTextAppendFormat(const TCHAR* format, ...) {
  ASSERT1(format);

  CString log_string;

  va_list arguments;
  va_start(arguments, format);
  SafeCStringFormatV(&log_string, format, arguments);
  va_end(arguments);

  __mutexScope(model()->lock());
  SafeCStringAppendFormat(&event_log_text_, _T("App=%s, Ver=%s, %s\n"),
                          app_guid_string().GetString(),
                          current_version()->version().GetString(),
                          log_string.GetString());
}

void App::AddPingEvent(const PingEventPtr& ping_event) {
  __mutexScope(model()->lock());
  ping_events_.push_back(ping_event);

  CORE_LOG(L3, (_T("[ping event added][%s]"), ping_event->ToString()));

  VERIFY_SUCCEEDED(app_bundle()->BuildAndPersistPing());
}

HRESULT App::CheckGroupPolicy() const {
  __mutexScope(model()->lock());

  if (is_update_) {
    if (!ConfigManager::Instance()->CanUpdateApp(
             app_guid_,
             !app_bundle_->is_auto_update())) {
      if (ConfigManager::Instance()->GetEffectivePolicyForAppUpdates(
          app_guid_, NULL) == kPolicyAutomaticUpdatesOnly) {
        // This return code allows Omaha clients to show a message indicating
        // Manual Updates are disabled, but Automatic Updates are enabled. This
        // is to reassure end-users that administrators have enabled automatic
        // updates.
        return GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL;
      }

      return GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY;
    }
  } else {
    if (!ConfigManager::Instance()->CanInstallApp(app_guid_,
                                                  app_bundle_->is_machine())) {
      return GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY;
    }
  }

  return S_OK;
}

CString App::GetTargetChannel() const {
  __mutexScope(model()->lock());

  return ConfigManager::Instance()->GetTargetChannel(app_guid_, NULL);
}

bool App::IsRollbackToTargetVersionAllowed() const {
  __mutexScope(model()->lock());

  return ConfigManager::Instance()->IsRollbackToTargetVersionAllowed(app_guid_,
                                                                     NULL);
}

CString App::GetTargetVersionPrefix() const {
  __mutexScope(model()->lock());

  return ConfigManager::Instance()->GetTargetVersionPrefix(app_guid_, NULL);
}

void App::UpdateNumBytesDownloaded(uint64 num_bytes) {
  __mutexScope(model()->lock());

  CORE_LOG(L3, (_T("[RecordDownloadedBytes][new bytes downloaded: %llu]"),
                num_bytes));
  num_bytes_downloaded_ += num_bytes;
}

uint64 App::num_bytes_downloaded() const {
  __mutexScope(model()->lock());
  return num_bytes_downloaded_;
}

uint64 App::GetPackagesTotalSize() const {
  __mutexScope(model()->lock());

  uint64 total_size = 0;
  const size_t num_packages = working_version_->GetNumberOfPackages();
  for (size_t i = 0; i < num_packages; ++i) {
    total_size += working_version_->GetPackage(i)->expected_size();
  }

  return total_size;
}

void App::SetCurrentTimeAs(TimeMetricType time_type) {
  __mutexScope(model()->lock());
  ASSERT1(time_type < TIME_METRICS_MAX);
  time_metrics_[time_type] = GetCurrentMsTime();
}


int App::GetTimeDifferenceMs(TimeMetricType time_start_metric_type,
                             TimeMetricType time_end_metric_type) const {
  __mutexScope(model()->lock());

  uint64 start_time_ms = time_metrics_[time_start_metric_type];
  uint64 end_time_ms = time_metrics_[time_end_metric_type];
  if (start_time_ms == 0 || start_time_ms > end_time_ms) {
    return 0;
  }

  return static_cast<int>(end_time_ms - start_time_ms);
}

int App::GetDownloadTimeMs() const {
  return GetTimeDifferenceMs(TIME_DOWNLOAD_START, TIME_DOWNLOAD_COMPLETE);
}

int App::GetInstallTimeMs() const {
  return GetTimeDifferenceMs(TIME_INSTALL_START, TIME_INSTALL_COMPLETE);
}

int App::GetUpdateCheckTimeMs() const {
  return GetTimeDifferenceMs(TIME_UPDATE_CHECK_START,
                             TIME_UPDATE_CHECK_COMPLETE);
}

int App::GetTimeSinceUpdateAvailable() const {
  __mutexScope(model()->lock());
  if (time_metrics_[TIME_UPDATE_AVAILABLE] == 0 ||
      time_metrics_[TIME_CANCELLED] == 0) {
    return -1;
  }
  return GetTimeDifferenceMs(TIME_UPDATE_AVAILABLE, TIME_CANCELLED);
}

int App::GetTimeSinceDownloadStart() const {
  __mutexScope(model()->lock());
  if (time_metrics_[TIME_DOWNLOAD_START] == 0 ||
      time_metrics_[TIME_CANCELLED] == 0) {
    return -1;
  }
  return GetTimeDifferenceMs(TIME_DOWNLOAD_START, TIME_CANCELLED);
}

//
// State transition methods.
// These should not do anything except acquire the lock if appropriate and call
// the corresponding AppState method.
//

// This is the first transition. EULA acceptance must have been set by now.
// Fail so that client developers realize quickly that something is wrong.
// Otherwise, they might ship a client that installs apps that never update.
void App::QueueUpdateCheck() {
  __mutexScope(model()->lock());

  ASSERT1(is_eula_accepted_ != TRISTATE_NONE);
  if (is_eula_accepted_ == TRISTATE_NONE) {
    CString message;
    StringFormatter formatter(app_bundle_->display_language());
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALL_FAILED, &message));
    Error(ErrorContext(GOOPDATE_E_CALL_UNEXPECTED), message);
  }

  app_state_->QueueUpdateCheck(this);
}

void App::PreUpdateCheck(xml::UpdateRequest* update_request) {
  ASSERT1(update_request);
  __mutexScope(model()->lock());
  app_state_->PreUpdateCheck(this, update_request);
}

void App::PostUpdateCheck(HRESULT result,
                          xml::UpdateResponse* update_response) {
  ASSERT1(update_response);
  __mutexScope(model()->lock());
  app_state_->PostUpdateCheck(this, result, update_response);
}

void App::QueueDownload() {
  __mutexScope(model()->lock());
  app_state_->QueueDownload(this);
}

void App::QueueDownloadOrInstall() {
  __mutexScope(model()->lock());
  app_state_->QueueDownloadOrInstall(this);
}

// Does not take the lock because this is a blocking call.
void App::Download(DownloadManagerInterface* download_manager) {
  app_state_->Download(this, download_manager);
}

void App::Downloading() {
  __mutexScope(model()->lock());
  app_state_->Downloading(this);
}

void App::DownloadComplete() {
  __mutexScope(model()->lock());
  app_state_->DownloadComplete(this);
}

void App::MarkReadyToInstall() {
  __mutexScope(model()->lock());
  app_state_->MarkReadyToInstall(this);
}

void App::QueueInstall() {
  __mutexScope(model()->lock());
  app_state_->QueueInstall(this);
}

// Does not take the lock because this is a blocking call.
void App::Install(InstallManagerInterface* install_manager) {
  app_state_->Install(this, install_manager);
}

void App::Installing() {
  __mutexScope(model()->lock());
  app_state_->Installing(this);
}

void App::ReportInstallerComplete(const InstallerResultInfo& result_info) {
  __mutexScope(model()->lock());
  app_state_->ReportInstallerComplete(this,
                                      result_info);
}

void App::Pause() {
  __mutexScope(model()->lock());
  return app_state_->Pause(this);
}

void App::Cancel() {
  __mutexScope(model()->lock());
  return app_state_->Cancel(this);
}

void App::Error(const ErrorContext& error_context, const CString& message) {
  __mutexScope(model()->lock());
  app_state_->Error(this, error_context, message);
}

void App::ChangeState(fsm::AppState* app_state) {
  ASSERT1(app_state);
  ASSERT1(model()->IsLockedByCaller());
  CurrentState existing_state = app_state_->state();
  app_state_.reset(app_state);
  PingEventPtr ping_event(
      app_state->CreatePingEvent(this, existing_state));
  if (ping_event.get()) {
    AddPingEvent(ping_event);
  }
}

void App::SetError(const ErrorContext& error_context, const CString& message) {
  ASSERT1(FAILED(error_context.error_code));
  ASSERT1(!message.IsEmpty());
  ASSERT1(model()->IsLockedByCaller());

  error_context_      = error_context;
  completion_message_ = message;

  is_canceled_ = (error_context_.error_code == GOOPDATE_E_CANCELLED);
  completion_result_  = is_canceled_ ? PingEvent::EVENT_RESULT_CANCELLED :
                                       PingEvent::EVENT_RESULT_ERROR;
}

void App::SetNoUpdate(const ErrorContext& error_context,
                      const CString& message) {
  ASSERT1(!message.IsEmpty());
  ASSERT1(model()->IsLockedByCaller());

  error_context_      = error_context;
  completion_message_ = message;

  const bool is_deferred_update =
      (error_context_.error_code == GOOPDATE_E_UPDATE_DEFERRED);
  completion_result_  = is_deferred_update ?
                        PingEvent::EVENT_RESULT_UPDATE_DEFERRED :
                        PingEvent::EVENT_RESULT_SUCCESS;
}

void App::SetInstallerResult(const InstallerResultInfo& result_info) {
  ASSERT1(result_info.type != INSTALLER_RESULT_UNKNOWN);
  ASSERT1(!result_info.text.IsEmpty());
  ASSERT1(model()->IsLockedByCaller());

  completion_message_               = result_info.text;
  installer_result_code_            = result_info.code;
  installer_result_extra_code1_     = result_info.extra_code1;
  post_install_launch_command_line_ =
      result_info.post_install_launch_command_line;
  post_install_url_                 = result_info.post_install_url;
  post_install_action_              = result_info.post_install_action;

  switch (result_info.type) {
    case INSTALLER_RESULT_SUCCESS: {
      error_context_.error_code = S_OK;

      // TODO(omaha3): Determine whether a reboot is required. See TODO in
      // InstallerWrapper.
      const bool is_reboot_required = false;
      completion_result_ = is_reboot_required ?
                           PingEvent::EVENT_RESULT_SUCCESS_REBOOT :
                           PingEvent::EVENT_RESULT_SUCCESS;

      // We do not know whether Goopdate has succeeded because its installer has
      // not completed.
      if (!::IsEqualGUID(kGoopdateGuid, app_guid_)) {
        AppManager::Instance()->PersistSuccessfulInstall(*this);
      }
      break;
    }
    case INSTALLER_RESULT_ERROR_MSI:
      completion_result_ = PingEvent::EVENT_RESULT_INSTALLER_ERROR_MSI;
      error_context_.error_code = GOOPDATEINSTALL_E_INSTALLER_FAILED;
      break;
    case INSTALLER_RESULT_ERROR_SYSTEM:
      completion_result_ = PingEvent::EVENT_RESULT_INSTALLER_ERROR_SYSTEM;
      error_context_.error_code = GOOPDATEINSTALL_E_INSTALLER_FAILED;
      break;
    case INSTALLER_RESULT_ERROR_OTHER:
      completion_result_ = PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER;
      error_context_.error_code = GOOPDATEINSTALL_E_INSTALLER_FAILED;
      break;
    case INSTALLER_RESULT_UNKNOWN:
    default:
      ASSERT1(false);
      completion_result_ = PingEvent::EVENT_RESULT_ERROR;
      error_context_.error_code = E_FAIL;
  }
}

CString App::GetInstallData() const {
  __mutexScope(model()->lock());

  ASSERT1(state() >= STATE_UPDATE_AVAILABLE &&
          state() <= STATE_INSTALL_COMPLETE);

  if (!client_install_data_.IsEmpty()) {
    return client_install_data_;
  }

  return server_install_data_;
}

// IApp.
STDMETHODIMP AppWrapper::get_currentVersion(IDispatch** current_version) {
  __mutexScope(model()->lock());
  return AppVersionWrapper::Create(controlling_ptr(),
                                   wrapped_obj()->current_version(),
                                   current_version);
}

STDMETHODIMP AppWrapper::get_nextVersion(IDispatch** next_version) {
  __mutexScope(model()->lock());
  return AppVersionWrapper::Create(controlling_ptr(),
                                   wrapped_obj()->next_version(),
                                   next_version);
}

STDMETHODIMP AppWrapper::get_command(BSTR command_id, IDispatch** command) {
  __mutexScope(model()->lock());
  ASSERT1(command);
  *command = NULL;
  AppCommandModel* unwrapped_command = wrapped_obj()->command(command_id);
  if (!unwrapped_command) {
    return S_FALSE;
  }
  return AppCommandWrapper::Create(controlling_ptr(),
                                   unwrapped_command,
                                   command);
}

// IApp.
STDMETHODIMP AppWrapper::get_appId(BSTR* app_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_appId(app_id);
}

STDMETHODIMP AppWrapper::get_pv(BSTR* pv) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_pv(pv);
}

STDMETHODIMP AppWrapper::put_pv(BSTR pv) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_pv(pv);
}

STDMETHODIMP AppWrapper::get_language(BSTR* language) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_language(language);
}

STDMETHODIMP AppWrapper::put_language(BSTR language) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_language(language);
}

STDMETHODIMP AppWrapper::get_ap(BSTR* ap) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_ap(ap);
}

STDMETHODIMP AppWrapper::put_ap(BSTR ap) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_ap(ap);
}

STDMETHODIMP AppWrapper::get_ttToken(BSTR* tt_token) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_ttToken(tt_token);
}

STDMETHODIMP AppWrapper::put_ttToken(BSTR tt_token) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_ttToken(tt_token);
}

STDMETHODIMP AppWrapper::get_iid(BSTR* iid) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_iid(iid);
}

STDMETHODIMP AppWrapper::put_iid(BSTR iid) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_iid(iid);
}

STDMETHODIMP AppWrapper::get_brandCode(BSTR* brand_code) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_brandCode(brand_code);
}

STDMETHODIMP AppWrapper::put_brandCode(BSTR brand_code) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_brandCode(brand_code);
}

STDMETHODIMP AppWrapper::get_clientId(BSTR* client_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_clientId(client_id);
}

STDMETHODIMP AppWrapper::put_clientId(BSTR client_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_clientId(client_id);
}

STDMETHODIMP AppWrapper::get_labels(BSTR* labels) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_labels(labels);
}

STDMETHODIMP AppWrapper::put_labels(BSTR labels) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_labels(labels);
}

STDMETHODIMP AppWrapper::get_referralId(BSTR* referral_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_referralId(referral_id);
}

STDMETHODIMP AppWrapper::put_referralId(BSTR referral_id) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_referralId(referral_id);
}

STDMETHODIMP AppWrapper::get_installTimeDiffSec(UINT* install_time_diff_sec) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_installTimeDiffSec(install_time_diff_sec);
}

STDMETHODIMP AppWrapper::get_isEulaAccepted(VARIANT_BOOL* is_eula_accepted) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_isEulaAccepted(is_eula_accepted);
}

STDMETHODIMP AppWrapper::put_isEulaAccepted(VARIANT_BOOL is_eula_accepted) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_isEulaAccepted(is_eula_accepted);
}

STDMETHODIMP AppWrapper::get_displayName(BSTR* display_name) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_displayName(display_name);
}

STDMETHODIMP AppWrapper::put_displayName(BSTR display_name) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_displayName(display_name);
}

STDMETHODIMP AppWrapper::get_browserType(UINT* browser_type) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_browserType(browser_type);
}

STDMETHODIMP AppWrapper::put_browserType(UINT browser_type) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_browserType(browser_type);
}

STDMETHODIMP AppWrapper::get_clientInstallData(BSTR* data) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_clientInstallData(data);
}

STDMETHODIMP AppWrapper::put_clientInstallData(BSTR data) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_clientInstallData(data);
}

STDMETHODIMP AppWrapper::get_serverInstallDataIndex(BSTR* index) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_serverInstallDataIndex(index);
}

STDMETHODIMP AppWrapper::put_serverInstallDataIndex(BSTR index) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_serverInstallDataIndex(index);
}

STDMETHODIMP AppWrapper::get_untrustedData(BSTR* data) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_untrustedData(data);
}

STDMETHODIMP AppWrapper::put_untrustedData(BSTR data) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_untrustedData(data);
}

STDMETHODIMP AppWrapper::get_usageStatsEnable(UINT* usage_stats_enable) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_usageStatsEnable(usage_stats_enable);
}

STDMETHODIMP AppWrapper::put_usageStatsEnable(UINT usage_stats_enable) {
  __mutexScope(model()->lock());
  return wrapped_obj()->put_usageStatsEnable(usage_stats_enable);
}

STDMETHODIMP AppWrapper::get_currentState(IDispatch** current_state_disp) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_currentState(current_state_disp);
}


// Sets app's app_state to state. Used by unit tests to set up the state to the
// correct precondition for the test case. App friends this function, allowing
// it to call the private member function.
void SetAppStateForUnitTest(App* app, fsm::AppState* state) {
  ASSERT1(app);
  ASSERT1(state);
  __mutexScope(app->model()->lock());
  app->ChangeState(state);
}

}  // namespace omaha
