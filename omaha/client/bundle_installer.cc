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

#include "omaha/client/bundle_installer.h"
#include <atlsafe.h>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/client/client_utils.h"
#include "omaha/client/help_url_builder.h"
#include "omaha/client/resource.h"
#include "omaha/client/shutdown_events.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/update3_utils.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

namespace internal {

CString GetAppDisplayName(IApp* app) {
  ASSERT1(app);

  CComBSTR app_name;
  HRESULT hr = app->get_displayName(&app_name);
  if (SUCCEEDED(hr)) {
    ASSERT1(app_name.Length());
    if (app_name.Length()) {
      return CString(app_name);
    }
  } else {
    CORE_LOG(LW, (_T("[get_displayName failed][0x%08x]"), hr));
  }
  return client_utils::GetDefaultApplicationName();
}

CString BuildAppNameList(const std::vector<CString>& app_names) {
  ASSERT1(!app_names.empty());

  CString list = app_names[0];
  for (size_t i = 1; i < app_names.size(); ++i) {
    list.FormatMessage(IDS_APPLICATION_NAME_CONCATENATION, list, app_names[i]);
  }

  return list;
}

CompletionCodes ConvertPostInstallActionToCompletionCode(
    PostInstallAction post_install_action,
    bool is_browser_type_supported,
    bool is_error_state) {
  switch (post_install_action) {
    case POST_INSTALL_ACTION_EXIT_SILENTLY_ON_LAUNCH_COMMAND:
      return COMPLETION_CODE_EXIT_SILENTLY_ON_LAUNCH_COMMAND;

    case POST_INSTALL_ACTION_LAUNCH_COMMAND:
      return COMPLETION_CODE_LAUNCH_COMMAND;

    case POST_INSTALL_ACTION_RESTART_BROWSER:
      if (is_browser_type_supported) {
        return COMPLETION_CODE_RESTART_BROWSER;
      } else {
        return COMPLETION_CODE_RESTART_BROWSER_NOTICE_ONLY;
      }

    case POST_INSTALL_ACTION_RESTART_ALL_BROWSERS:
      if (is_browser_type_supported) {
        return COMPLETION_CODE_RESTART_ALL_BROWSERS;
      } else {
        return COMPLETION_CODE_RESTART_ALL_BROWSERS_NOTICE_ONLY;
      }

    case POST_INSTALL_ACTION_REBOOT:
      // We don't support reboot, always notice_only
      return COMPLETION_CODE_REBOOT_NOTICE_ONLY;

    case POST_INSTALL_ACTION_EXIT_SILENTLY:
      return COMPLETION_CODE_EXIT_SILENTLY;

    case POST_INSTALL_ACTION_DEFAULT:
      if (is_error_state) {
        return COMPLETION_CODE_ERROR;
      } else {
        return COMPLETION_CODE_SUCCESS;
      }

    default:
      ASSERT1(false);
      return COMPLETION_CODE_SUCCESS;
  }
}

HRESULT GetCompletionInformation(IApp* app,
                                 CurrentState* current_state,
                                 AppCompletionInfo* completion_info,
                                 bool is_browser_type_supported) {
  ASSERT1(app);
  ASSERT1(current_state);
  ASSERT1(completion_info);

  *current_state = STATE_INIT;

  // If the COM call fails, which may be why we encountered an error, this
  // could be "Google Application", which is weird, especially in the mixed
  // results UI.
  completion_info->display_name = internal::GetAppDisplayName(app);

  CComBSTR app_id;
  HRESULT hr = app->get_appId(&app_id);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_appId failed][0x%08x]"), hr));
    return hr;
  }

  completion_info->app_id = app_id;

  CComPtr<ICurrentState> icurrent_state;
  hr = update3_utils::GetAppCurrentState(app, current_state, &icurrent_state);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetNextVersionState failed][0x%08x]"), hr));
    return hr;
  }

  ASSERT1(*current_state == STATE_INSTALL_COMPLETE ||
          *current_state == STATE_NO_UPDATE ||
          *current_state == STATE_ERROR);

  hr = icurrent_state->get_errorCode(&completion_info->error_code);
  if (FAILED(hr)) {
    return hr;
  }

  LONG extra_code1 = 0;
  hr = icurrent_state->get_extraCode1(&extra_code1);
  if (FAILED(hr)) {
    return hr;
  }
  completion_info->extra_code1 = static_cast<int>(extra_code1);

  CComBSTR message;
  hr = icurrent_state->get_completionMessage(&message);
  if (FAILED(hr)) {
    return hr;
  }
  completion_info->completion_message = message;

  LONG installer_result = 0;
  hr = icurrent_state->get_installerResultCode(&installer_result);
  if (FAILED(hr)) {
    return hr;
  }
  completion_info->installer_result_code = installer_result;

  VARIANT_BOOL is_canceled = VARIANT_TRUE;
  hr = icurrent_state->get_isCanceled(&is_canceled);
  if (FAILED(hr)) {
    return hr;
  }
  completion_info->is_canceled = (is_canceled == VARIANT_TRUE);

  CComBSTR post_install_launch_command_line;
  hr = icurrent_state->get_postInstallLaunchCommandLine(
           &post_install_launch_command_line);
  if (FAILED(hr)) {
    return hr;
  }
  completion_info->post_install_launch_command_line =
      post_install_launch_command_line;

  CORE_LOG(L5, (_T("[GetCompletionInformation][%s][state: %u][code: 0x%08x]")
                _T("[extra code: %d][message: '%s'][installer: %u]"),
                app_id, *current_state,
                completion_info->error_code,
                completion_info->extra_code1,
                message, installer_result));

  CComBSTR post_install_url;
  hr = icurrent_state->get_postInstallUrl(&post_install_url);
  if (FAILED(hr)) {
    return hr;
  }
  completion_info->post_install_url = post_install_url;

  LONG post_install_action = static_cast<LONG>(POST_INSTALL_ACTION_DEFAULT);
  hr = icurrent_state->get_postInstallAction(&post_install_action);
  if (FAILED(hr)) {
    return hr;
  }
  completion_info->completion_code = ConvertPostInstallActionToCompletionCode(
      static_cast<PostInstallAction>(post_install_action),
      is_browser_type_supported,
      *current_state == STATE_ERROR);

  ASSERT1(SUCCEEDED(completion_info->error_code) ||
          completion_info->installer_result_code == 0 ||
          completion_info->error_code == GOOPDATEINSTALL_E_INSTALLER_FAILED);

  return S_OK;
}

void GetAppCompletionMessage(IApp* app,
                             AppCompletionInfo* app_info,
                             bool is_browser_type_supported) {
  ASSERT1(app);
  ASSERT1(app_info);

  CurrentState current_state = STATE_INIT;

  HRESULT hr = GetCompletionInformation(app,
                                        &current_state,
                                        app_info,
                                        is_browser_type_supported);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[GetCompletionInformation failed][0x%08x]"), hr));
    // Treat the failure as an app failure.
    app_info->completion_message.FormatMessage(
        IDS_INSTALL_FAILED_WITH_ERROR_CODE,
        hr);
    app_info->error_code = hr;
    app_info->extra_code1 = 0;
    return;
  }

  ASSERT1(current_state == STATE_INSTALL_COMPLETE ||
          current_state == STATE_NO_UPDATE ||
          current_state == STATE_ERROR);
  ASSERT1(!app_info->completion_message.IsEmpty());
  ASSERT1(current_state != STATE_NO_UPDATE ||
          app_info->error_code == S_OK     ||
          app_info->error_code == GOOPDATE_E_UPDATE_DEFERRED);

  app_info->is_noupdate = current_state == STATE_NO_UPDATE;

  // If it is not a success or noupdate, we must report a failure in error_code.
  if (current_state != STATE_INSTALL_COMPLETE &&
      current_state != STATE_NO_UPDATE) {
    ASSERT1(FAILED(app_info->error_code));
    if (SUCCEEDED(app_info->error_code)) {
      app_info->error_code = E_FAIL;
      app_info->extra_code1 = 0;
    }
  }

  if (!app_info->completion_message.IsEmpty()) {
    return;
  }

  ASSERT(false, (_T("There should always be a completion message.")));

  // The message is empty. Return a default message.
  if (current_state == STATE_INSTALL_COMPLETE) {
    app_info->completion_message.LoadString(
        IDS_APPLICATION_INSTALLED_SUCCESSFULLY);
  } else if (current_state == STATE_NO_UPDATE) {
    VERIFY1(app_info->completion_message.LoadString(IDS_NO_UPDATE_RESPONSE));
  } else {
    ASSERT1(current_state == STATE_ERROR);
    app_info->completion_message.FormatMessage(
        IDS_INSTALL_FAILED_WITH_ERROR_CODE,
        E_FAIL);
  }
}

CString BuildResultStringForApps(uint32 group_name_resource_id,
                                 const std::vector<CString>& apps) {
  CString app_result_string;
  ASSERT1(!apps.empty());

  // The header (i.e. "Failed:") should be in bold.
  const TCHAR* const kAppListHeaderFormatOpen = _T("<b>");
  const TCHAR* const kAppListHeaderFormatClose = _T("</b>");

  CString apps_list = internal::BuildAppNameList(apps);
  app_result_string.FormatMessage(group_name_resource_id,
                                  kAppListHeaderFormatOpen,
                                  kAppListHeaderFormatClose,
                                  apps_list);
  return app_result_string;
}

// TODO(omaha): If we end up leaving is_noupdate in AppCompletionInfo,
// eliminate is_only_no_update parameter.
CString GetBundleCompletionMessage(
    const CString& bundle_name,
    const std::vector<AppCompletionInfo>& apps_info,
    bool is_only_no_update,
    bool is_canceled) {
  ASSERT1(!apps_info.empty());
  // TODO(omaha): Enable this assert if GetUpdateAllAppsBundleName() is
  // changed to a different string.
  // ASSERT(bundle_name != SHORT_COMPANY_NAME _T(" Application"),
  //        (_T("Do not pass default bundle name to this function.")));

  CString bundle_message;

  if (is_only_no_update) {
    VERIFY1(bundle_message.LoadString(IDS_NO_UPDATE_RESPONSE));
    return bundle_message;
  }

  std::vector<CString> succeeded_apps;
  std::vector<CString> failed_apps;
  std::vector<CString> canceled_apps;
  CString first_failure_message;
  for (size_t i = 0; i < apps_info.size(); ++i) {
    const AppCompletionInfo& app_info = apps_info[i];
    ASSERT1(!app_info.display_name.IsEmpty());
    ASSERT1(!app_info.completion_message.IsEmpty());

    if (SUCCEEDED(app_info.error_code)) {
      succeeded_apps.push_back(app_info.display_name);
    } else if (app_info.is_canceled) {
      canceled_apps.push_back(app_info.display_name);
    } else {
      failed_apps.push_back(app_info.display_name);

      // For now, we only display the first error message when all apps fail.
      // Remember that message.
      if (first_failure_message.IsEmpty()) {
        first_failure_message = app_info.completion_message;
      }
    }
  }

  CString canceled_apps_str;
  if (!canceled_apps.empty()) {
    canceled_apps_str = BuildResultStringForApps(
        IDS_BUNDLE_MIXED_RESULTS_CANCELED_APPS, canceled_apps);
  }

  CString succeeded_apps_str;
  if (!succeeded_apps.empty()) {
    succeeded_apps_str = BuildResultStringForApps(
        IDS_BUNDLE_MIXED_RESULTS_SUCCEEDED_APPS, succeeded_apps);
  }
  CString failed_apps_str;
  if (!failed_apps.empty()) {
    failed_apps_str = BuildResultStringForApps(
        IDS_BUNDLE_MIXED_RESULTS_FAILED_APPS, failed_apps);
  }

  // For mixed results, display the succeeded, failed and canceled app lists on
  // their own lines below the main message with a newline between the message
  // and lists.
  // Added "<B> </B>" (or any different text format) to the end of the layout
  // strings so that all string components on the left can be compacted together
  // which make it look nicer on the completion dialog.
  const TCHAR* const kLayoutForTwoGroups = _T("%s\n%s    %s<B> </B>");
  const TCHAR* const kLayoutForThreeGroups = _T("%s\n%s    %s    %s<B> </B>");

  if (!failed_apps_str.IsEmpty()) {
    // At least one app fails to install, display a failure message.
    uint32 message_id =
        failed_apps.size() == 1 ?
            IDS_BUNDLE_MIXED_RESULTS_MESSAGE_ONE_FAILURE :
            IDS_BUNDLE_MIXED_RESULTS_MESSAGE_MULTIPLE_FAILURES;
    CString message;
    VERIFY1(message.LoadString(message_id));

    if (!succeeded_apps.empty() && !canceled_apps.empty()) {
      SafeCStringFormat(&bundle_message, kLayoutForThreeGroups,
                        message,
                        succeeded_apps_str,
                        failed_apps_str,
                        canceled_apps_str);
    } else if (!succeeded_apps.empty()) {
      SafeCStringFormat(&bundle_message, kLayoutForTwoGroups,
                        message,
                        succeeded_apps_str,
                        failed_apps_str);
    } else if (!canceled_apps.empty()) {
      SafeCStringFormat(&bundle_message, kLayoutForTwoGroups,
                        message,
                        failed_apps_str,
                        canceled_apps_str);
    } else {
      bundle_message = first_failure_message;
    }
  } else if (!canceled_apps_str.IsEmpty()) {
    // No failed app, but some are canceled.
    if (!succeeded_apps_str.IsEmpty()) {
      CString message;
      VERIFY1(message.LoadString(
          IDS_BUNDLE_INSTALLED_SUCCESSFULLY_AFTER_CANCEL));
      SafeCStringFormat(&bundle_message, kLayoutForTwoGroups,
                        message,
                        succeeded_apps_str,
                        canceled_apps_str);
    } else {
      // Only canceled app, no UI will be displayed.
      VERIFY1(bundle_message.LoadString(IDS_CANCELED));
    }
  } else {
    // All successes. Display a client-specific completion message that includes
    // the bundle name.
    // There is no special handling of apps with noupdate.
    ASSERT1(succeeded_apps.size() == apps_info.size());
    ASSERT1(first_failure_message.IsEmpty());
    if (is_canceled) {
      VERIFY1(bundle_message.LoadString(
          IDS_BUNDLE_INSTALLED_SUCCESSFULLY_AFTER_CANCEL));
    } else if (bundle_name.IsEmpty()) {
      VERIFY1(
          bundle_message.LoadString(IDS_APPLICATION_INSTALLED_SUCCESSFULLY));
    } else {
      VERIFY1(bundle_message.LoadString(IDS_BUNDLE_INSTALLED_SUCCESSFULLY));
    }
  }

  ASSERT1(!bundle_message.IsEmpty());
  return bundle_message;
}

}  // namespace internal

BundleInstaller::BundleInstaller(HelpUrlBuilder* help_url_builder,
                                 bool is_update_all_apps,
                                 bool is_update_check_only,
                                 bool is_browser_type_supported)
    : observer_(NULL),
      help_url_builder_(help_url_builder),
      parent_window_(NULL),
      state_(kInit),
      result_(E_UNEXPECTED),
      is_canceled_(false),
      is_handling_message_(false),
      is_update_all_apps_(is_update_all_apps),
      is_update_check_only_(is_update_check_only),
      is_browser_type_supported_(is_browser_type_supported) {
}

BundleInstaller::~BundleInstaller() {
  Uninitialize();
}

LRESULT BundleInstaller::OnTimer(UINT msg,
                                 WPARAM wparam,
                                 LPARAM,
                                 BOOL& handled) {  // NOLINT
  if (is_handling_message_) {
    ASSERT(false, (_T("[Reentrancy detected]")));
    return 0;
  }
  is_handling_message_ = true;

  VERIFY1(msg == WM_TIMER);
  VERIFY1(wparam == kPollingTimerId);

  if (!PollServer()) {
    CORE_LOG(L6, (_T("[BundleInstaller::OnTimer][Stopping polling timer]")));

    // Ignore return value. KillTimer does not remove WM_TIMER messages already
    // posted to the message queue.
    KillTimer(kPollingTimerId);
  }

  is_handling_message_ = false;
  handled = true;
  return 0;
}

HRESULT BundleInstaller::Initialize() {
  CORE_LOG(L3, (_T("[BundleInstaller::Initialize]")));

  // Create a message-only window for the timer. It is not visible,
  // has no z-order, cannot be enumerated, and does not receive broadcast
  // messages. The window simply dispatches messages.
  const TCHAR kWndName[] = _T("{139455DE-14E2-4d54-93B5-9E6ADDC04B4E}");
  if (!Create(HWND_MESSAGE, NULL, kWndName)) {
    return HRESULTFromLastError();
  }

  if (!SetTimer(kPollingTimerId, kPollingTimerPeriodMs)) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

void BundleInstaller::Uninitialize() {
  if (IsWindow()) {
    // This may fail if it was already killed when the bundle completed.
    KillTimer(kPollingTimerId);

    DestroyWindow();
  }
}

void BundleInstaller::SetBundleParentWindow(HWND parent_window) {
  ASSERT1(parent_window);
  parent_window_ = parent_window;
}

HRESULT BundleInstaller::ListenToShutdownEvent(bool is_machine) {
  ASSERT1(!shutdown_callback_.get());
  HRESULT hr = ShutdownEvents::CreateShutdownHandler(
      is_machine, this, &shutdown_callback_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("CreateShutdownHandler failed][0x%08x]"), hr));
  }

  return hr;
}

void BundleInstaller::StopListenToShutdownEvent(bool is_machine) {
  UNREFERENCED_PARAMETER(is_machine);
  shutdown_callback_.reset();
}

HRESULT BundleInstaller::InstallBundle(bool is_machine,
                                       bool listen_to_shutdown_event,
                                       IAppBundle* app_bundle,
                                       InstallProgressObserver* observer) {
  ASSERT1(app_bundle);
  ASSERT1(!app_bundle_);
  app_bundle_.Attach(app_bundle);
  app_bundle_->put_parentHWND(reinterpret_cast<ULONG_PTR>(parent_window_));

  observer_ = observer;

  if (listen_to_shutdown_event) {
    ListenToShutdownEvent(is_machine);
  }

  _pAtlModule->Lock();

  message_loop_.Run();
  CORE_LOG(L2, (_T("[message_loop_.Run() returned]")));

  if (listen_to_shutdown_event) {
    StopListenToShutdownEvent(is_machine);
  }

  observer_ = NULL;

  // Installer should not hold any reference to app bundle after installation.
  ASSERT1(!app_bundle_);

  CORE_LOG(L1, (_T("InstallBundle returning][0x%08x]"), result()));
  return result();
}

// Shutdown() is called from a thread in the OS threadpool. The PostMessage
// marshals the call over to the UI thread, which is where DoClose needs to be
// (and is) called from.
LRESULT BundleInstaller::OnClose(UINT,
                                 WPARAM,
                                 LPARAM,
                                 BOOL& handled) {         // NOLINT
  CORE_LOG(L3, (_T("[BundleInstaller::OnClose]")));

  if (is_handling_message_) {
    ASSERT(false, (_T("[Reentrancy detected]")));
    return 0;
  }
  is_handling_message_ = true;

  DoClose();

  is_handling_message_ = false;
  handled = true;
  return 0;
}

// Assumes that we can call OnComplete multiple times.
void BundleInstaller::DoClose() {
  CORE_LOG(L1, (_T("[BundleInstaller::DoClose]")));
  if (kComplete != state_) {
    CORE_LOG(L1,
             (_T("[UI closed before install completed. Likely canceled.]")));
    DoCancel();
  }
}

void BundleInstaller::DoExit() {
  CORE_LOG(L1, (_T("[BundleInstaller::DoExit]")));
  ASSERT(state_ == kComplete, (_T("[State not complete yet, cannot exit!]")));

  _pAtlModule->Unlock();
}

void BundleInstaller::DoCancel() {
  CancelBundle();
  is_canceled_ = true;
}

// Unless a catastrophic/unrecoverable error occurs, bundle processing should
// continue, resulting in NotifyBundleInstallComplete() being called and S_OK
// being returned up the callstack.
//
// The following classes of errors can be returned by DoPollServer().
// * Errors returned by methods in this class (e.g. E_FAIL).
//   The state_ may already have been set to kComplete
//   * (e.g. GOOPDATE_E_NO_UPDATE_RESPONSE) or not (e.g. E_FAIL).
// * Errors returned by utility methods (e.g. by goopdate_utils methods).
// * COM errors returned due to API or sever failures.
// * Errors returned by COM methods (e.g. by install()).
//
// Specifically note that app errors are not returned up the call stack. These
// are handled and reported by NotifyBundleInstallComplete().
//
// Thus, for all errors returned by DoPollServer() except those returned by COM
// methods (not property methods), the client knows the error description.
// TODO(omaha3): What should we do for the COM method errors? There is currently
// no error API for the bundle. Also, errors returned by these calls may not
// be Omaha-specific errors. They could be COM errors (e.g. server unavailable),
// in which case calling a COM method would not work.
//
// DoPollServer() handles some client-side errors
// (e.g. GOOPDATE_E_NO_UPDATE_RESPONSE) by setting state_ to kComplete. All
// other errors must be handled by calling Complete().
// In all error cases, the bundle must be canceled.
bool BundleInstaller::PollServer() {
  HRESULT hr = DoPollServer();

  // Handle the error unless it has already been handled.
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DoPollServer failed][0x%08x][%d]"), hr, state_));

    CancelBundle();

    if (state_ != kComplete) {
      CString message;
      message.FormatMessage(IDS_INSTALL_FAILED_WITH_ERROR_CODE, hr);
      BundleCompletionInfo bundle_info(COMPLETION_CODE_ERROR, hr, message);
      Complete(bundle_info);
    }
  }

  return state_ != kComplete;
}

HRESULT BundleInstaller::result() {
  ASSERT1(kComplete == state_);
  return result_;
}

// Polls the server for the state of the job and updates the UI.
// Not thread safe. There should only be one installation per process. Do we
// need to worry about multiple WM_TIMER events at the same time or does the
// message loop ensure this doesn't happen?
HRESULT BundleInstaller::DoPollServer() {
  CORE_LOG(L6, (_T("[BundleInstaller::DoPollServer][%u]"), state_));

  if (!observer_) {
    CORE_LOG(LW, (_T("[BundleInstaller::DoPollServer][observer_ is NULL]")));
    return S_FALSE;
  }

  switch (state_) {
    case kInit:
      return HandleInitState();
    case kProcessing:
      return HandleProcessingState();
    case kComplete:
      return S_OK;
    default:
      ASSERT1(false);
      return E_FAIL;
  }
}

// Checks whether the update check is complete, and if so, gets the number of
// apps with updates available.
HRESULT BundleInstaller::HandleUpdateAvailable() {
  CORE_LOG(L3, (_T("[BundleInstaller::HandleUpdateAvailable]")));
  ASSERT1(!apps_.empty());
  ASSERT1(app_bundle_);

  if (is_update_all_apps_) {
    // Nothing to do. The bundle will automatically continue on to the download.
    return S_OK;
  }

  // The bundle will not automatically continue. Initiate the download and
  // install if appropriate.

  VARIANT_BOOL is_busy = VARIANT_TRUE;
  HRESULT hr = app_bundle_->isBusy(&is_busy);
  if (FAILED(hr)) {
    return hr;
  }

  if (is_busy) {
    // An update is available, but other apps may still be being processed.
    // Wait until bundle is not busy to indicate all apps have been processed.
    return S_OK;
  }

  // The only purpose of this call now is to call NotifyUpdateAvailable().
  int num_updates = 0;
  hr = HandleUpdateCheckResults(&num_updates);
  if (FAILED(hr)) {
    return hr;
  }
  CORE_LOG(L2, (_T("[Update check complete][updates: %d]"), num_updates));
  ASSERT1(num_updates);

  if (is_update_check_only_) {
    return NotifyBundleUpdateCheckOnlyComplete();
  }

  // TODO(omaha): Do we handle an unexpected number of apps correctly?
  // (i.e. apps_.size() != num_updates)
  // This includes one of n apps reporting no update during an install.

  return app_bundle_->install();
}

// Populates apps_. This must be done here because updateAllApps() adds apps
// to app_bundle_.
HRESULT BundleInstaller::HandleInitState() {
  CORE_LOG(L3, (_T("[BundleInstaller::HandleInitState]")));
  ASSERT1(observer_);
  ASSERT1(app_bundle_);

  state_ = kProcessing;
  HRESULT hr = is_update_all_apps_ ?
                   app_bundle_->updateAllApps() :
                   app_bundle_->checkForUpdate();
  if (FAILED(hr)) {
    return hr;
  }
  observer_->OnCheckingForUpdate();

  long count = 0;  // NOLINT
  hr = app_bundle_->get_Count(&count);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(count > 0);

  for (long i = 0; i != count; ++i) {  // NOLINT
    CComPtr<IApp> app;
    hr = update3_utils::GetApp(app_bundle_, i, &app);
    if (FAILED(hr)) {
      return hr;
    }
    apps_.push_back(AdaptIApp(app));
  }

  ASSERT1(!apps_.empty());
  return S_OK;
}

// Iterates through the apps until it finds one in a non-terminal state.
// If all apps are in a terminal state, calls NotifyBundleInstallComplete().
// Assumes that apps are processed serially in order. If this changes, we need
// to change the algorithm to avoid a bad UI experience.
// TODO(omaha): For things like creating a log that might be visible in the UI,
// we would need to guarantee that we always report each major state for each
// app. This would also require changing this algorithm. In addition, the COM
// server would need to return information for all previous AppStates
// (i.e. download progress while in the install phase).
HRESULT BundleInstaller::HandleProcessingState() {
  CORE_LOG(L6, (_T("[BundleInstaller::HandleProcessingState]")));
  ASSERT1(observer_);
  ASSERT1(!apps_.empty());

  for (size_t i = 0; i < apps_.size(); ++i) {
    CurrentState current_state = STATE_INIT;
    CComPtr<ICurrentState> icurrent_state;
    const ComPtrIApp& app = apps_[i];
    HRESULT hr = update3_utils::GetAppCurrentState(app,
                                                   &current_state,
                                                   &icurrent_state);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetNextVersionState failed][0x%08x]"), hr));
      return hr;
    }

    switch (current_state) {
      case STATE_INSTALL_COMPLETE:
      case STATE_NO_UPDATE:
      case STATE_ERROR:
        // Terminal state - nothing to do for this app. Check the next app.
        continue;
      case STATE_WAITING_TO_CHECK_FOR_UPDATE:
      case STATE_CHECKING_FOR_UPDATE:
        return S_OK;
      case STATE_UPDATE_AVAILABLE:
        return HandleUpdateAvailable();
      case STATE_WAITING_TO_DOWNLOAD: {
        CComBSTR app_id;
        VERIFY_SUCCEEDED(app->get_appId(&app_id));
        observer_->OnWaitingToDownload(app_id.m_str,
                                       internal::GetAppDisplayName(app));
        return S_OK;
      }
      case STATE_RETRYING_DOWNLOAD:
        ASSERT(false, (_T("Unsupported")));
        return S_OK;  // Keep checking in order to be forwards compatible.
      case STATE_DOWNLOADING:
      case STATE_DOWNLOAD_COMPLETE:
        return NotifyDownloadProgress(app, icurrent_state);
      case STATE_EXTRACTING:
      case STATE_APPLYING_DIFFERENTIAL_PATCH:
      case STATE_READY_TO_INSTALL:
      case STATE_WAITING_TO_INSTALL:
        return NotifyWaitingToInstall(app);
      case STATE_INSTALLING:
        return NotifyInstallProgress(app, icurrent_state);
      case STATE_PAUSED:
        ASSERT(false, (_T("Unsupported")));
        return S_OK;  // Keep checking in order to be forwards compatible.
      case STATE_INIT:
      default:
        ASSERT1(false);
        return S_OK;  // Keep checking in order to be forwards compatible.
                      // Cannot support new terminal states, though.
    }
  }

  // No apps were in non-terminal states. The bundle may still be busy, though,
  // because app states are updated separately from the bundle state.
  // TODO(omaha): Should we wait for the bundle to complete? If not, we may need
  // to add appropriate waits and checks for any completion UI bundle actions
  // that require that the AppBundle is not busy.

  return NotifyBundleInstallComplete();
}

HRESULT BundleInstaller::NotifyUpdateAvailable(IApp* app) {
  CORE_LOG(L3, (_T("[BundleInstaller::NotifyUpdateAvailable]")));
  ASSERT1(app);
  ASSERT1(observer_);

  CComPtr<IAppVersion> next_version;
  HRESULT hr = update3_utils::GetNextAppVersion(app, &next_version);
  if (FAILED(hr)) {
    return hr;
  }

  CComBSTR ver;
  hr = next_version->get_version(&ver);
  if (FAILED(hr)) {
    return hr;
  }

  CORE_LOG(L3, (_T("[Next Version Update Available][%s]"), CString(ver)));

  CComBSTR app_id;
  VERIFY_SUCCEEDED(app->get_appId(&app_id));

  // TODO(omaha3): Until we force app teams to provide a version, the string
  // may be empty.
  observer_->OnUpdateAvailable(app_id.m_str,
                               internal::GetAppDisplayName(app),
                               CString(ver));
  return S_OK;
}

HRESULT BundleInstaller::NotifyDownloadProgress(IApp* app,
                                                ICurrentState* icurrent_state) {
  CORE_LOG(L3, (_T("[BundleInstaller::NotifyDownloadProgress]")));
  ASSERT1(icurrent_state);
  ASSERT1(observer_);

  int time_remaining_ms = kCurrentStateProgressUnknown;
  int percentage = 0;
  time64 next_retry_time = 0;
  GetAppDownloadProgress(icurrent_state,
                         &time_remaining_ms,
                         &percentage,
                         &next_retry_time);
  CComBSTR app_id;
  VERIFY_SUCCEEDED(app->get_appId(&app_id));

  if (next_retry_time != 0) {
    observer_->OnWaitingRetryDownload(app_id.m_str,
                                      internal::GetAppDisplayName(app),
                                      next_retry_time);
  } else {
    observer_->OnDownloading(app_id.m_str,
                             internal::GetAppDisplayName(app),
                             time_remaining_ms,
                             percentage);
  }
  return S_OK;
}

// Starts the install unless the UI prevents the install from starting, in which
// case it remains in the same state to be checked again next cycle.
HRESULT BundleInstaller::NotifyWaitingToInstall(IApp* app) {
  CORE_LOG(L3, (_T("[BundleInstaller::NotifyWaitingToInstall]")));
  ASSERT1(app);
  ASSERT1(observer_);

  CComBSTR app_id;
  VERIFY_SUCCEEDED(app->get_appId(&app_id));

  // can_start_install is ignored because download and install are no longer
  // discrete phases.
  bool can_start_install = false;
  observer_->OnWaitingToInstall(app_id.m_str,
                                internal::GetAppDisplayName(app),
                                &can_start_install);

  return S_OK;
}

HRESULT BundleInstaller::NotifyInstallProgress(IApp* app,
                                               ICurrentState* icurrent_state) {
  CORE_LOG(L3, (_T("[BundleInstaller::NotifyInstallProgress]")));
  ASSERT1(app);
  ASSERT1(icurrent_state);
  ASSERT1(observer_);

  int time_remaining_ms = kCurrentStateProgressUnknown;
  int percentage = 0;
  GetAppInstallProgress(icurrent_state, &time_remaining_ms, &percentage);

  CComBSTR app_id;
  VERIFY_SUCCEEDED(app->get_appId(&app_id));
  observer_->OnInstalling(app_id.m_str,
                          internal::GetAppDisplayName(app),
                          time_remaining_ms,
                          percentage);
  return S_OK;
}

// Only used by legacy OnDemand.
HRESULT BundleInstaller::NotifyBundleUpdateCheckOnlyComplete() {
  CORE_LOG(L3, (_T("[BundleInstaller::NotifyBundleUpdateCheckOnlyComplete]")));

  BundleCompletionInfo info(COMPLETION_CODE_SUCCESS,
                            S_OK,
                            _T("OK"));  // Not used by legacy OnDemand.

  Complete(info);
  return S_OK;
}

// Assumes the AppBundle has completed and all apps are in a terminal state.
// In other words, AppBundle::Cancel does not need to be called.
// For now, append the completion message(s) from each app. If any app failed,
// display the failure UI and set this objects result to the app error.
// TODO(omaha3): Improve the UI for bundles. It does not currently handle
// restart browser, etc. Nor is the output production-ready as it just appends
// the completion strings and assumes L2R. We at least need prettier printing,
// maybe each message on its own line. Also, our error messages are inconsistent
// in whether they specify the app's name.
HRESULT BundleInstaller::NotifyBundleInstallComplete() {
  CORE_LOG(L3, (_T("[BundleInstaller::NotifyBundleInstallComplete]")));
  ASSERT1(!apps_.empty());
  ASSERT1(app_bundle_);

  bool is_only_no_update = true;

  std::vector<AppCompletionInfo> apps_info;
  HRESULT bundle_result = S_OK;

  // Get the completion info for each app and set the bundle result.
  for (size_t i = 0; i < apps_.size(); ++i) {
    const ComPtrIApp& app = apps_[i];
    AppCompletionInfo app_info;
    internal::GetAppCompletionMessage(app,
                                      &app_info,
                                      is_browser_type_supported_);

    CORE_LOG(L1, (_T("[App completion][%Iu][%s]"), i, app_info.ToString()));
    apps_info.push_back(app_info);

    is_only_no_update &= app_info.is_noupdate;

    ASSERT1(bundle_result == S_OK || FAILED(bundle_result));
    if (FAILED(app_info.error_code) && SUCCEEDED(bundle_result)) {
      // This is the first app failure. Use this as the result.
      bundle_result = app_info.error_code;
    }
  }

  ASSERT1(bundle_result == S_OK || !is_only_no_update);

  CComBSTR bundle_name;
  if (FAILED(app_bundle_->get_displayName(&bundle_name))) {
    bundle_name.Empty();
  }

  CString current_bundle_message = internal::GetBundleCompletionMessage(
                                       CString(bundle_name),
                                       apps_info,
                                       is_only_no_update,
                                       is_canceled_);
  ASSERT1(!current_bundle_message.IsEmpty());
  CompletionCodes completion_code = COMPLETION_CODE_SUCCESS;
  if (FAILED(bundle_result)) {
    completion_code = COMPLETION_CODE_ERROR;
  } else if (is_canceled_) {
    // User tried to cancel but bundle is installed.
    completion_code = COMPLETION_CODE_INSTALL_FINISHED_BEFORE_CANCEL;
  }
  BundleCompletionInfo bundle_info(completion_code,
                                   bundle_result,
                                   current_bundle_message);
  bundle_info.apps_info = apps_info;  // Copying simplifies the code above.

  // The exit code will be non-zero if any app failed to install.
  // TODO(omaha3): What if apps have different settings? It seems anyone calling
  // Omaha would expect to get an error code in this case. We need to make sure
  // this doesn't cause undesirable behavior in the parent process(es).
  Complete(bundle_info);
  return S_OK;
}

// If an update is available, this method also sets relevant information from
// the update response.
// This function, and thus, NotifyUpdateAvailable() is only called if using
// a phased install where the bundle waits after the update check (in other
// words, !is_update_all_apps_). Currently, NotifyUpdateAvailable() only does
// something in the legacy OnDemand case, so this is okay.
HRESULT BundleInstaller::HandleUpdateCheckResults(int* num_updates) {
  CORE_LOG(L1, (_T("[BundleInstaller::HandleUpdateCheckResults]")));
  ASSERT1(num_updates);

  *num_updates = 0;

  for (size_t i = 0; i < apps_.size(); ++i) {
    CurrentState current_state = STATE_INIT;
    CComPtr<ICurrentState> icurrent_state;
    const ComPtrIApp& app = apps_[i];
    HRESULT hr = update3_utils::GetAppCurrentState(app,
                                                   &current_state,
                                                   &icurrent_state);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetAppCurrentState failed][0x%08x]"), hr));
      return hr;
    }

    if (current_state == STATE_NO_UPDATE || current_state == STATE_ERROR) {
      // Continue to process other apps.
      // The error information, if applicable, will be reported elsewhere.
      continue;
    } else if (current_state != STATE_UPDATE_AVAILABLE) {
      // The update check may not be complete or may be in an unexpected state.
      ASSERT1(false);
      return E_FAIL;
    }

    ++*num_updates;
    VERIFY_SUCCEEDED(NotifyUpdateAvailable(app));
  }

  return S_OK;
}

// Assumes icurrent_state represents an app in one of the downloading states.
// TODO(omaha3): Since this method does not check the current state, it's
// possible to be in Download Complete or later but not report 100%. The server
// should ensure it reports 100% and 0 time in these cases.
void BundleInstaller::GetAppDownloadProgress(ICurrentState* icurrent_state,
                                             int* time_remaining_ms,
                                             int* percentage,
                                             time64* next_retry_time) {
  ASSERT1(icurrent_state);
  ASSERT1(time_remaining_ms);
  ASSERT1(percentage);

  LONG local_time_remaining_ms = kCurrentStateProgressUnknown;
  if (FAILED(icurrent_state->get_downloadTimeRemainingMs(
          &local_time_remaining_ms))) {
    local_time_remaining_ms = kCurrentStateProgressUnknown;
  }

  int local_percentage = 0;
  ULONG bytes = 0;
  ULONG bytes_total = 0;
  if (FAILED(icurrent_state->get_bytesDownloaded(&bytes)) ||
      FAILED(icurrent_state->get_totalBytesToDownload(&bytes_total))) {
    local_percentage = 0;
  } else {
    ASSERT1(bytes <= bytes_total);
    local_percentage = static_cast<int>(100ULL * bytes / bytes_total);
    ASSERT1(0 <= local_percentage && local_percentage <= 100);
  }


  ULONGLONG local_next_retry_time = 0;
  if (FAILED(icurrent_state->get_nextRetryTime(&local_next_retry_time))) {
    local_next_retry_time = 0;
  }

  *time_remaining_ms = local_time_remaining_ms;
  *percentage = local_percentage;
  *next_retry_time = static_cast<time64>(local_next_retry_time);

  // TODO(omaha3): For now, this client treats extracting and patching as part
  // of downloading. Add UI support for these phases.

  CORE_LOG(L4, (_T("[AppDownloadProgress]")
                _T("[bytes %u][bytes_total %u][percentage %d][ms %d]"),
                bytes, bytes_total, *percentage, *time_remaining_ms));
}

void BundleInstaller::GetAppInstallProgress(ICurrentState* icurrent_state,
                                            int* time_remaining_ms,
                                            int* percentage) {
  ASSERT1(icurrent_state);
  ASSERT1(time_remaining_ms);
  ASSERT1(percentage);

  LONG local_time_remaining_ms = kCurrentStateProgressUnknown;
  VERIFY_SUCCEEDED(
      icurrent_state->get_installTimeRemainingMs(&local_time_remaining_ms));
  LONG local_percentage = kCurrentStateProgressUnknown;
  VERIFY_SUCCEEDED(icurrent_state->get_installProgress(&local_percentage));

  ASSERT1(local_percentage <= 100);
  *time_remaining_ms = local_time_remaining_ms;
  *percentage = local_percentage;

  CORE_LOG(L4, (_T("[AppInstallProgress][percentage %d][ms %d]"),
                *percentage, *time_remaining_ms));
}

void BundleInstaller::CancelBundle() {
  CORE_LOG(L1, (_T("[BundleInstaller::CancelBundle]")));
  if (app_bundle_) {
    VERIFY_SUCCEEDED(app_bundle_->stop());
  }
}

// error_code is copied to result_, which is the return code for this object.
void BundleInstaller::Complete(const BundleCompletionInfo& bundle_info) {
  CORE_LOG(L1, (_T("[BundleInstaller::Complete][%s]"), bundle_info.ToString()));
  ASSERT1(observer_);
  ASSERT1(!bundle_info.bundle_completion_message.IsEmpty());

  CString help_url;
  if (bundle_info.completion_code == COMPLETION_CODE_ERROR) {
    std::vector<HelpUrlBuilder::AppResult> app_install_results;
    for (size_t i = 0; i < bundle_info.apps_info.size(); ++i) {
      const AppCompletionInfo& info = bundle_info.apps_info[i];
      // TODO(omaha3): Pass info.extra_code to HelpUrlBuilder as well so that
      // the help URL has both extra code and installer result code.
      app_install_results.push_back(
          HelpUrlBuilder::AppResult(info.app_id,
                                    info.error_code,
                                    info.installer_result_code));
    }

    if (help_url_builder_.get()) {
      VERIFY_SUCCEEDED(help_url_builder_->BuildUrl(app_install_results,
                                                    &help_url));
      ASSERT1(!help_url.IsEmpty());
    }
  }

  // Set result_ and state_ before calling OnComplete on the observer.
  // Otherwise, we end up calling BundleInstaller::Complete() recursively from
  // DoClose().
  result_ = bundle_info.bundle_result;
  state_ = kComplete;

  ReleaseAppBundle();

  // TODO(omaha3): We need to expose some more items to the observer, such as
  // install_manifest.install_actions[].success_action. There are a lot of
  // things we need to expose together: success action, restart browser,
  // terminate all browsers, url, and maybe others. Let's take
  // the opportunity to standardize these even if the registry and config APIs
  // are not ideal (i.e. success_url should not imply an action as it does in
  // the config).

  ObserverCompletionInfo observer_info(bundle_info.completion_code);
  // TODO(omaha3): Consider moving the creation of the bundle completion
  // message from this class to the observer.
  observer_info.completion_text = bundle_info.bundle_completion_message;
  observer_info.help_url = help_url;
  observer_info.apps_info = bundle_info.apps_info;

  observer_->OnComplete(observer_info);
}

// Omaha event pings are sent in AppBundle destructor. Release app_bundle_ and
// its related interfaces explicitly so that the pings can be sent sooner.
void BundleInstaller::ReleaseAppBundle() {
  CORE_LOG(L3, (_T("[ReleaseAppBundle]")));
  apps_.clear();
  app_bundle_ = NULL;
}

}  // namespace omaha
