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

#include "omaha/goopdate/install_manager.h"
#include <vector>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/install_manifest.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/installer_wrapper.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"

namespace omaha {

namespace {

// Number of tries when the MSI service is busy.
// Updates are silent so we can wait longer.
const int kNumMsiAlreadyRunningInteractiveMaxTries = 4;  // Up to 35 seconds.
const int kNumMsiAlreadyRunningSilentMaxTries      = 7;  // Up to 6.25 minutes.

// TODO(omaha): there can be more install actions for each install event.
bool GetInstallActionForEvent(
    const std::vector<xml::InstallAction>& install_actions,
    xml::InstallAction::InstallEvent install_event,
    xml::InstallAction* action) {
  ASSERT1(action);

  for (size_t i = 0; i < install_actions.size(); ++i) {
    if (install_actions[i].install_event == install_event) {
      *action = install_actions[i];
      return true;
    }
  }

  return false;
}

}  // namespace

InstallManager::InstallManager(const Lockable* model_lock, bool is_machine)
    : model_lock_(model_lock),
      is_machine_(is_machine) {
  CORE_LOG(L3, (_T("[InstallManager::InstallManager][%d]"), is_machine_));

  install_working_dir_ =
      is_machine ?
      ConfigManager::Instance()->GetMachineInstallWorkingDir() :
      ConfigManager::Instance()->GetUserInstallWorkingDir();
  CORE_LOG(L3, (_T("[install_working_dir][%s]"), install_working_dir()));

  VERIFY_SUCCEEDED(CreateDir(install_working_dir_, NULL));
  VERIFY_SUCCEEDED(DeleteDirectoryContents(install_working_dir_));

  installer_wrapper_.reset(new InstallerWrapper(is_machine_));
}

InstallManager::~InstallManager() {
  CORE_LOG(L3, (_T("[InstallManager::~InstallManager]")));
}

HRESULT InstallManager::Initialize() {
  return installer_wrapper_->Initialize();
}

CString InstallManager::install_working_dir() const {
  __mutexScope(model_lock_);
  return install_working_dir_;
}

// For each app, set the state to STATE_INSTALLING, install it, and update the
// state of the model after it completes.
void InstallManager::InstallApp(App* app,  const CString& dir) {
  CORE_LOG(L3, (_T("[InstallManager::InstallApp][0x%p]"), app));
  ASSERT1(app);

  const ConfigManager& cm = *ConfigManager::Instance();
  // TODO(omaha): Since we don't currently have is_manual, check the least
  // restrictive case of true. It would be nice if we had is_manual. We'll see.
  ASSERT(SUCCEEDED(app->CheckGroupPolicy()),
         (_T("Installing/updating app for which this is not allowed.")));
  ASSERT(app->is_eula_accepted() ||
         app->is_install() && app->app_bundle()->is_offline_install(),
         (_T("update/online install of app for which EULA is not accepted.")));

  // TODO(omaha3): This needs to be set/passed per app/bundle.
  const int priority = app->app_bundle()->priority();
  const int num_tries = (priority < INSTALL_PRIORITY_HIGH) ?
                             kNumMsiAlreadyRunningSilentMaxTries :
                             kNumMsiAlreadyRunningInteractiveMaxTries;
  installer_wrapper_->set_num_tries_when_msi_busy(num_tries);

  AppVersion* next_version = app->next_version();
  ASSERT1(app->app_bundle()->is_machine() == is_machine_);

  const CString current_version_string = app->current_version()->version();

  HANDLE primary_token(app->app_bundle()->primary_token());

  HRESULT hr = InstallApp(is_machine_,
                          primary_token,
                          current_version_string,
                          *model_lock_,
                          installer_wrapper_.get(),
                          app,
                          dir);

  CORE_LOG(LE, (_T("[InstallApp returned][0x%p][0x%08x]"), app, hr));

  app->LogTextAppendFormat(_T("Install result=0x%08x"), hr);

  ASSERT1(FAILED(hr) == (app->state() == STATE_ERROR));
}

HRESULT InstallManager::InstallApp(bool is_machine,
                                   HANDLE user_token,
                                   const CString& existing_version,
                                   const Lockable& model_lock,
                                   InstallerWrapper* installer_wrapper,
                                   App* app,
                                   const CString& dir) {
  UNREFERENCED_PARAMETER(is_machine);
  ASSERT1(installer_wrapper);
  ASSERT1(app);

  const bool is_update = app->is_update();

  CString display_name;
  GUID app_guid = {0};
  CString installer_path;
  // TODO(omaha3): Consider generating the installerdata file external to the
  // InstallerWrapper and just adding the path to the arguments.
  CString manifest_arguments;
  CString installer_data;
  CString expected_version;

  AppManager& app_manager = *AppManager::Instance();
  __mutexScope(app_manager.GetRegistryStableStateLock());

  // TODO(omaha): If this does not get much simpler, extract method.
  AppVersion& next_version = *(app->next_version());
  ASSERT1(app->app_bundle()->is_machine() == is_machine);

  CString language = app->app_bundle()->display_language();

  // TODO(omaha): review the need for locking below.
  __mutexBlock(model_lock) {
    app->Installing();

    app_guid = app->app_guid();

    // The first package is always the Package Manager for the app.
    // TODO(omaha3): Use program_to_run here instead of the installer_path. This
    // will introduce complexity such as having to find the full path and
    // handling the case where the file is not in the cache.
    ASSERT1(next_version.GetNumberOfPackages() > 0);
    if (next_version.GetNumberOfPackages() <= 0) {
      HRESULT hr = E_FAIL;
      const CString message = InstallerWrapper::GetMessageForError(hr,
                                                                   CString(),
                                                                   language);
      app->Error(ErrorContext(hr), message);
      return hr;
    }
    const Package& package_manager = *(next_version.GetPackage(0));
    installer_path = ConcatenatePath(dir, package_manager.filename());

    xml::InstallAction action;
    const bool is_event_found = GetInstallActionForEvent(
        next_version.install_manifest()->install_actions,
        is_update ? xml::InstallAction::kUpdate : xml::InstallAction::kInstall,
        &action);
    ASSERT1(is_event_found);
    if (is_event_found) {
      manifest_arguments = action.program_arguments;

      // If this is an Omaha self-update, append a switch to the argument list
      // to set the session ID.
      if (is_update && ::IsEqualGUID(app_guid, kGoopdateGuid)) {
        SafeCStringAppendFormat(&manifest_arguments, _T(" /%s \"%s\""),
                                kCmdLineSessionId,
                                app->app_bundle()->session_id());
      }
    }

    installer_data = app->GetInstallData();

    expected_version = next_version.install_manifest()->version;

    // TODO(omaha3): All app key registry writes and reads must be protected by
    // some lock to prevent race conditions caused by multiple bundles
    // installing the same app. This includes while writing the pre-install
    // data, while the installer is running, and while checking application
    // registration (basically the rest of this method). An app-specific lock
    // similar to the app install lock in Omaha 2 seems to be appropriate.
    // (Omaha 2 only took that during install - not updates - though.)
    // Some app installers may also check the state of other apps (i.e. to check
    // for version compatibility). Should we protect this case as well?
    // Is it safest to use the installer lock for this? What is the performance
    // impact. If we do use the installer lock, it would need to be acquired
    // here instead of in the InstallerWrapper::InstallApp path.
    if (!is_update) {
      HRESULT hr = app_manager.WritePreInstallData(*app);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[AppManager::WritePreInstallData failed][0x%08x]")));
        const CString message =
            InstallerWrapper::GetMessageForError(hr, CString(), language);
        app->Error(ErrorContext(hr), message);
        return hr;
      }
    }
  }

  const int install_priority = app->app_bundle()->priority();
  OPT_LOG(L1, (
      _T("[Installing][display name: %s][app id: %s][installer path: %s]")
      _T("[manifest args: %s][installer data: %s][untrusted data: %s]")
      _T("[priority: %d]"),
      app->display_name(),
      GuidToString(app_guid),
      installer_path,
      manifest_arguments,
      installer_data,
      app->untrusted_data(),
      install_priority));

  InstallerResultInfo result_info;

  app->SetCurrentTimeAs(App::TIME_INSTALL_START);
  HRESULT hr = installer_wrapper->InstallApp(user_token,
                                             app_guid,
                                             installer_path,
                                             manifest_arguments,
                                             installer_data,
                                             language,
                                             app->untrusted_data(),
                                             install_priority,
                                             &result_info);
  app->SetCurrentTimeAs(App::TIME_INSTALL_COMPLETE);

  OPT_LOG(L1, (_T("[InstallApp returned][0x%x][%s][type:%d][code: %d][%s][%s]"),
               hr, GuidToString(app_guid), result_info.type, result_info.code,
               result_info.text, result_info.post_install_launch_command_line));

  __mutexScope(model_lock);

  if (SUCCEEDED(hr)) {
    ASSERT1(result_info.type == INSTALLER_RESULT_SUCCESS);
    // Skip checking application registration for Omaha self-updates because the
    // installer has not completed.
    if (!::IsEqualGUID(kGoopdateGuid, app_guid)) {
      hr = app_manager.ReadInstallerRegistrationValues(app);
      if (SUCCEEDED(hr)) {
        hr = installer_wrapper->CheckApplicationRegistration(
            app_guid,
            next_version.version(),
            expected_version,
            existing_version,
            is_update);
      }
    }
  } else {
    ASSERT1(result_info.type != INSTALLER_RESULT_SUCCESS);
    ASSERT1(!result_info.text.IsEmpty() ||
            result_info.type == INSTALLER_RESULT_UNKNOWN);
  }

  if (FAILED(hr)) {
    // If we failed the install job and the product wasn't registered, it's safe
    // to delete the ClientState key.  We need to remove it because it contains
    // data like "ap", browsertype, language, etc. that need to be cleaned up in
    // case user tries to install again in the future.
    if (!is_update && !app_manager.IsAppRegistered(app->app_guid())) {
      app_manager.RemoveClientState(app->app_guid());
    }

    if (hr == GOOPDATEINSTALL_E_INSTALLER_FAILED) {
      ASSERT1(!result_info.text.IsEmpty());
      app->ReportInstallerComplete(result_info);
    } else {
      // TODO(omaha3): If we end up having the installer filename above, pass it
      // instead of installer_path.
      const CString message = InstallerWrapper::GetMessageForError(
                                  hr, installer_path, language);
      app->Error(ErrorContext(hr, result_info.extra_code1), message);
    }

    return hr;
  }

  PopulateSuccessfulInstallResultInfo(app, &result_info);

  app->ReportInstallerComplete(result_info);
  return S_OK;
}

// Call this function only when installer succeeded.
// This function sets the post install action and url based on installer result
// info and app manifest. Note that the action may already have been set by
// InstallerWrapper::GetInstallerResultHelper() in some cases. This function
// only sets the action when necessary.
void InstallManager::PopulateSuccessfulInstallResultInfo(
    const App* app,
    InstallerResultInfo* result_info) {
  ASSERT1(result_info);
  ASSERT1(result_info->type == INSTALLER_RESULT_SUCCESS);
  ASSERT1(app);
  ASSERT1(app->next_version()->install_manifest());

  xml::InstallAction action;
  if (GetInstallActionForEvent(
          app->next_version()->install_manifest()->install_actions,
          xml::InstallAction::kPostInstall,
          &action)) {
    result_info->post_install_url = action.success_url;

    if (result_info->post_install_launch_command_line.IsEmpty() &&
        action.success_action == SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD) {
      CORE_LOG(LW, (_T("[Success action specified launchcmd, but cmd empty]")));
    }

    if (result_info->post_install_action ==
            POST_INSTALL_ACTION_LAUNCH_COMMAND &&
        action.success_action == SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD) {
      result_info->post_install_action =
          POST_INSTALL_ACTION_EXIT_SILENTLY_ON_LAUNCH_COMMAND;
    } else if (result_info->post_install_action ==
        POST_INSTALL_ACTION_DEFAULT) {
      if (action.success_action == SUCCESS_ACTION_EXIT_SILENTLY) {
        result_info->post_install_action = POST_INSTALL_ACTION_EXIT_SILENTLY;
      } else if (!result_info->post_install_url.IsEmpty()) {
        result_info->post_install_action = action.terminate_all_browsers ?
            POST_INSTALL_ACTION_RESTART_ALL_BROWSERS :
            POST_INSTALL_ACTION_RESTART_BROWSER;
      }
    }
  }

  // Load message based on post install action if not overridden.
  if (result_info->text.IsEmpty()) {
    StringFormatter formatter(app->app_bundle()->display_language());
    VERIFY_SUCCEEDED(formatter.LoadString(
                          IDS_APPLICATION_INSTALLED_SUCCESSFULLY,
                          &result_info->text));
  }
}

}  // namespace omaha
