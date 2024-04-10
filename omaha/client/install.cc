// Copyright 2007-2010 Google Inc.
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

#include "omaha/client/install.h"
#include "omaha/client/install_internal.h"
#include "omaha/base/app_util.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/client/client_utils.h"
#include "omaha/client/install_self.h"
#include "omaha/client/resource.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/oem_install_utils.h"
#include "omaha/common/ping.h"
#include "omaha/setup/setup_metrics.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "omaha/ui/splash_screen.h"

namespace omaha {

namespace {

// Returns whether elevation is required.
bool IsElevationRequired(bool is_machine) {
  return is_machine && !vista_util::IsUserAdmin();
}

}  // namespace

namespace internal {

// TODO(omaha3): Make this elevate the metainstaller instead of
// GoogleUpdate.exe so the files are extracted to a secure location.
// May need to add all languages to the metainstaller's version resources so
// the user sees the localized string in the UAC.
// TODO(omaha3): We will need to save the metainstaller for OneClick
// cross-installs. We may need to change the metainstaller behavior to not
// use the tag if any command line args are provided. This will allow us to
// reuse a tagged metainstaller that we saved for other purposes, such as
// OneClick cross-installs.
// TODO(omaha3): The "metainstaller" may also be some type of wrapper around
// a differential update. We'll address that later. This wrapper should not
// need localized resource strings since it always runs silently.
HRESULT DoElevation(bool is_interactive,
                    bool is_install_elevated_instance,
                    const CString& cmd_line,
                    DWORD* exit_code) {
  ASSERT1(exit_code);
  ASSERT1(IsElevationRequired(true));
  ASSERT1(is_interactive);

  if (!is_interactive) {
    return GOOPDATE_E_SILENT_INSTALL_NEEDS_ELEVATION;
  }

  if (is_install_elevated_instance) {
    // This can happen if UAC is disabled. See http://b/1187784.
    CORE_LOG(LE, (_T("[Install elevated process requires elevation]")));
    return GOOPDATE_E_INSTALL_ELEVATED_PROCESS_NEEDS_ELEVATION;
  }

  HRESULT hr = ElevateAndWait(cmd_line, exit_code);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ElevateAndWait failed][%s][0x%08x]"), cmd_line, hr));
  }

  return hr;
}

// Assumes it is running with the necessary privileges.
HRESULT DoInstall(bool is_machine,
                  bool is_app_install,
                  bool is_eula_required,
                  bool is_oem_install,
                  bool is_enterprise_install,
                  const CString& current_version,
                  const CommandLineArgs& args,
                  const CString& session_id,
                  SplashScreen* splash_screen,
                  int* extra_code1,
                  bool* has_setup_succeeded,
                  bool* has_launched_handoff,
                  bool* has_ui_been_displayed) {
  ASSERT1(!IsElevationRequired(is_machine));
  ASSERT1(extra_code1);
  ASSERT1(splash_screen);
  ASSERT1(has_setup_succeeded);
  ASSERT1(has_launched_handoff);
  ASSERT1(has_ui_been_displayed);

  *extra_code1 = 0;

  // TODO(omaha3): We may need to take an "installing apps" lock here if
  // is_app_install. There was code in Omaha 2 that relied on the fact that
  // the Setup lock was held while the install worker was launched, even in
  // handoff scenarios. This allowed checking for the Setup lock and running
  // install workers to be sufficient to ensure that the number of Clients keys
  // was stable. Note that Omaha 2 also held the shutdown event while the worker
  // was launched.
  // TODO(omaha3): Consider taking the Setup lock in this file (via a public
  // method in Setup) and releasing it. This would allow us to address the above
  // issue and maybe the EULA not accepted issue.
  // TODO(omaha3): We may also want to call ShouldInstall after a Lock(0) to
  // determine whether we even need to display the splash screen or we can skip
  // it and Setup altogether and just launch the /handoff process.

  HRESULT hr = install_self::InstallSelf(is_machine,
                                         is_eula_required,
                                         is_oem_install,
                                         is_enterprise_install,
                                         current_version,
                                         args.install_source,
                                         args.extra,
                                         session_id,
                                         extra_code1);
  *has_setup_succeeded = SUCCEEDED(hr);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[InstallSelf failed][0x%08x]"), hr));
    return hr;
  }

  if (!is_app_install) {
    return S_OK;
  }

  hr = InstallApplications(is_machine,
                           is_eula_required,
                           args,
                           session_id,
                           splash_screen,
                           has_ui_been_displayed,
                           has_launched_handoff);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[InstallApplications failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT InstallApplications(bool is_machine,
                            bool is_eula_required,
                            const CommandLineArgs& args,
                            const CString& session_id,
                            SplashScreen* splash_screen,
                            bool* has_ui_been_displayed,
                            bool* has_launched_handoff) {
  ASSERT1(has_ui_been_displayed);
  ASSERT1(has_launched_handoff);

  *has_launched_handoff = false;

  CString offline_dir_name;
  bool is_offline_install = IsOfflineInstall(args.extra.apps) &&
                            CopyOfflineFiles(is_machine,
                                             args.extra.apps,
                                             &offline_dir_name);
  if (!is_offline_install && oem_install_utils::IsOemInstalling(is_machine)) {
    return GOOPDATE_E_OEM_WITH_ONLINE_INSTALLER;
  }
  if (!is_offline_install && is_eula_required) {
    return GOOPDATE_E_EULA_REQURED_WITH_ONLINE_INSTALLER;
  }

  // Start the handoff to install the app.
  scoped_process handoff_process;
  HRESULT hr = LaunchHandoffProcess(is_machine,
                                    offline_dir_name,
                                    args,
                                    session_id,
                                    address(handoff_process));
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to launch installed instance][0x%08x]"), hr));
    return hr;
  }

  *has_launched_handoff = true;

  OPT_LOG(L1, (_T("[Waiting for application install to complete]")));
  uint32 exit_code(0);
  hr = WaitForProcessExit(get(handoff_process),
                          splash_screen,
                          has_ui_been_displayed,
                          &exit_code);

  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed waiting for app install][0x%08x]"), hr));
    return hr;
  }
  if (exit_code) {
    OPT_LOG(LE, (_T("[Handoff exited with error][0x%08x]"), exit_code));
    ASSERT1(FAILED(exit_code));
    return exit_code;
  }

  return S_OK;
}

// The behavior depends on the OS:
//  1. OS < Vista  : Fail.
//  2. OS >= Vista : Try to elevate - causes a UAC dialog.
// We should be here only in case of initial machine installs when the user is
// not an elevated admin.
HRESULT ElevateAndWait(const CString& cmd_line, DWORD* exit_code) {
  OPT_LOG(L1, (_T("[Elevating][%s]"), cmd_line));
  ASSERT1(!vista_util::IsUserAdmin());
  ASSERT1(exit_code);

  if (!vista_util::IsVistaOrLater()) {
    // TODO(omaha): We could consider to ask for credentials here.
    // This TODO existed in Omaha 1. How would we even do this?
    CORE_LOG(LE, (_T("[Non Admin trying to install admin app]")));
    ++metric_setup_machine_app_non_admin;
    return GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP;
  }

  CString cmd_line_elevated(GetCmdLineTail(cmd_line));
  SafeCStringAppendFormat(&cmd_line_elevated, _T(" /%s"),
                          kCmdLineInstallElevated);

  HRESULT hr = goopdate_utils::StartElevatedMetainstaller(cmd_line_elevated,
                                                          exit_code);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Elevated metainstaller failed][%s][%#x]"), cmd_line, hr));

    // TODO(omaha3): Report hr somehow. Was reported in extra code in Omaha 2.
    if (vista_util::IsUserNonElevatedAdmin()) {
      return GOOPDATE_E_ELEVATION_FAILED_ADMIN;
    } else {
      return GOOPDATE_E_ELEVATION_FAILED_NON_ADMIN;
    }
  }

  return S_OK;
}

bool IsOfflineInstall(const std::vector<CommandLineAppArgs>& apps) {
  CORE_LOG(L3, (_T("[IsOfflineInstall]")));

  if (apps.empty()) {
    CORE_LOG(L3, (_T("[IsOfflineInstall][No Apps]")));
    return false;
  }

  CString manifest_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                          kOfflineManifestFileName);
  if (!File::Exists(manifest_path)) {
    CORE_LOG(L3, (_T("[IsOfflineInstall][Manifest does not exist]")));
    return false;
  }

  for (size_t i = 0; i < apps.size(); ++i) {
    const GUID& app_id = apps[i].app_guid;
    const CString app_id_string(GuidToString(app_id));

    if (!IsOfflineInstallForApp(app_id_string)) {
      CORE_LOG(L3, (_T("[IsOfflineInstall][No app files][%s]"), app_id_string));
      return false;
    }
  }

  return true;
}

bool IsOfflineInstallForApp(const CString& app_id) {
  CString current_directory(app_util::GetCurrentModuleDirectory());
  CString pattern;

  // Check if there are installer files named with the pattern "*.<app_id>".
  SafeCStringFormat(&pattern, _T("*.%s"), app_id);
  std::vector<CString> files;
  HRESULT hr = FindFiles(current_directory, pattern, &files);
  return SUCCEEDED(hr) && !files.empty();
}

bool CopyOfflineFiles(bool is_machine,
                      const std::vector<CommandLineAppArgs>& apps,
                      CString* offline_dir_name) {
  ASSERT1(offline_dir_name);
  offline_dir_name->Empty();

  if (apps.empty()) {
    return false;
  }

  GUID guid(GUID_NULL);
  VERIFY_SUCCEEDED(::CoCreateGuid(&guid));
  CString parent_offline_dir(
      is_machine ?
      ConfigManager::Instance()->GetMachineSecureOfflineStorageDir() :
      ConfigManager::Instance()->GetUserOfflineStorageDir());
  CString offline_dir_guid(GuidToString(guid));
  CString offline_path(ConcatenatePath(parent_offline_dir, offline_dir_guid));
  VERIFY_SUCCEEDED(CreateDir(offline_path, NULL));

  HRESULT hr = CopyOfflineManifest(offline_path);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[CopyOfflineManifest failed][0x%08x]"), hr));
    return false;
  }

  for (size_t i = 0; i < apps.size(); ++i) {
    const GUID& app_id = apps[i].app_guid;
    hr = CopyOfflineFilesForApp(GuidToString(app_id), offline_path);
    if (FAILED(hr)) {
      ASSERT(false, (_T("[CopyOfflineFilesForApp failed][0x%08x]"), hr));
      return false;
    }
  }

  CORE_LOG(L3, (_T("[CopyOfflineFiles done][%s]"), offline_path));
  *offline_dir_name = offline_dir_guid;
  return true;
}

HRESULT CopyOfflineManifest(const CString& offline_dir) {
  CORE_LOG(L3, (_T("[CopyOfflineManifest][%s]"), offline_dir));

  // Copy offline manifest into "<offline_dir>\<kOfflineManifestFileName>".
  CString setup_temp_dir(app_util::GetCurrentModuleDirectory());
  CString source_manifest_path = ConcatenatePath(setup_temp_dir,
                                                 kOfflineManifestFileName);
  if (!File::Exists(source_manifest_path)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }
  CString dest_manifest_path = ConcatenatePath(offline_dir,
                                               kOfflineManifestFileName);
  HRESULT hr = File::Copy(source_manifest_path, dest_manifest_path, true);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[File copy failed][%s][%s][0x%08x]"),
                   source_manifest_path, dest_manifest_path, hr));
    return hr;
  }

  return S_OK;
}

HRESULT CopyOfflineFilesForApp(const CString& app_id,
                               const CString& offline_dir) {
  CORE_LOG(L3, (_T("[CopyOfflineFilesForApp][%s][%s]"),
                 app_id, offline_dir));

  CString setup_temp_dir(app_util::GetCurrentModuleDirectory());
  CString pattern;
  // Copy the installer files that are named with the pattern "*.<app_id>" to
  // the directory "<offline_dir>\<app_id>".
  SafeCStringFormat(&pattern, _T("*.%s"), app_id);
  std::vector<CString> files;
  HRESULT hr = FindFiles(setup_temp_dir, pattern, &files);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[FindFiles failed][0x%08x]"), hr));
    return hr;
  }
  if (files.empty()) {
    CORE_LOG(LE, (_T("[FindFiles found no files]")));
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  CString offline_app_dir = ConcatenatePath(offline_dir, app_id);
  if (File::IsDirectory(offline_app_dir)) {
    VERIFY_SUCCEEDED(DeleteDirectoryFiles(offline_app_dir));
  } else {
    hr = CreateDir(offline_app_dir, NULL);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[CreateDir failed][%s]"), offline_app_dir));
      return hr;
    }
  }

  for (size_t i = 0; i < files.size(); ++i) {
    const CString& file_name = files[i];
    ASSERT1(file_name.GetLength() > app_id.GetLength());

    CPath renamed_file_name(file_name);
    renamed_file_name.RemoveExtension();
    ASSERT1(file_name.Left(file_name.GetLength() - app_id.GetLength() - 1) ==
            static_cast<const CString&>(renamed_file_name));
    CString new_file_path = ConcatenatePath(offline_app_dir, renamed_file_name);
    CORE_LOG(L4, (_T("[new_file_path][%s]"), new_file_path));

    CString source_file_path = ConcatenatePath(setup_temp_dir, file_name);
    hr = File::Copy(source_file_path, new_file_path, true);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[Copy failed][%s][%s]"),
                    source_file_path, new_file_path));
      return hr;
    }
  }

  return S_OK;
}


// TODO(omaha3): This implementation requires updating this code whenever a
// new option is added. We could do a string replace of "/install" with
// "/handoff", but we'd need to remove things such as /oem. Alternatively,
// allow a CommandLineBuilder to be populated with existing args. Doing
// replacement would allow us to only pass one copy of the command line to
// Install() instead of passing both the string and struct. Be sure to handle
// /appargs when making any changes.
// TODO(omaha): Extract the command line building and unit test it.
HRESULT LaunchHandoffProcess(bool is_machine,
                             const CString& offline_dir_name,
                             const CommandLineArgs& install_args,
                             const CString& session_id,
                             HANDLE* process) {   // process can be NULL.
  CORE_LOG(L2, (_T("[LaunchHandoffProcess]")));

  // The install source has been either specified or set to a default value by
  // the time the execution flow reaches this function. The install source is
  // propagated to the handoff process except in certain offline install cases,
  // such as a tagged offline installer or an offline installer that did not
  // have an install source.
  //
  // TODO(omaha): refactor so that the handling of the install source is
  // encapsulated in just one module.
  ASSERT1(!install_args.install_source.IsEmpty());
  ASSERT1(!install_args.extra_args_str.IsEmpty());

  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);

  builder.set_is_silent_set(install_args.is_silent_set);
  builder.set_is_always_launch_cmd_set(install_args.is_always_launch_cmd_set);
  builder.set_is_eula_required_set(install_args.is_eula_required_set);
  builder.set_is_enterprise_set(install_args.is_enterprise_set);
  builder.set_extra_args(install_args.extra_args_str);
  builder.set_app_args(install_args.app_args_str);
  builder.set_install_source(install_args.install_source);
  builder.set_session_id(session_id);

  if (!offline_dir_name.IsEmpty()) {
    HRESULT hr = builder.SetOfflineDirName(offline_dir_name);
    if (FAILED(hr)) {
      return hr;
    }

    const CString& install_source(install_args.install_source);
    if (install_source == kCmdLineInstallSource_InstallDefault ||
        install_source == kCmdLineInstallSource_TaggedMetainstaller) {
      builder.set_install_source(kCmdLineInstallSource_Offline);
    }
  }

  CString cmd_line = builder.GetCommandLineArgs();

  HRESULT hr = goopdate_utils::StartGoogleUpdateWithArgs(is_machine,
                                                         StartMode::kForeground,
                                                         cmd_line,
                                                         process);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Google Update hand off failed][%s][0x%08x]"),
                 cmd_line, hr));
    // TODO(omaha3): Report hr somehow. Was reported in extra code in Omaha 2.
    return GOOPDATE_E_HANDOFF_FAILED;
  }

  return S_OK;
}

HRESULT WaitForProcessExit(HANDLE process,
                           SplashScreen* splash_screen,
                           bool* has_ui_been_displayed,
                           uint32* exit_code) {
  CORE_LOG(L3, (_T("[WaitForProcessExit]")));
  ASSERT1(process);
  ASSERT1(exit_code);
  ASSERT1(has_ui_been_displayed);
  *exit_code = 0;

  int res = ::WaitForInputIdle(process, INFINITE);
  if (res == 0) {
    *has_ui_been_displayed = true;
  }
  if (splash_screen) {
    splash_screen->Dismiss();
  }

  res = ::WaitForSingleObject(process, INFINITE);
  ASSERT1(WAIT_OBJECT_0 == res);
  if (WAIT_FAILED == res) {
    DWORD error = ::GetLastError();
    CORE_LOG(LE, (_T("[::WaitForMultipleObjects failed][%u]"), error));
    return HRESULT_FROM_WIN32(error);
  }

  // Get the exit code.
  DWORD local_exit_code = 0;
  if (::GetExitCodeProcess(process, &local_exit_code)) {
    CORE_LOG(L2, (_T("[process exited][PID %u][exit code 0x%08x]"),
                  Process::GetProcessIdFromHandle(process), local_exit_code));
    *exit_code = local_exit_code;
  } else {
    DWORD error = ::GetLastError();
    CORE_LOG(LE, (_T("[::GetExitCodeProcess failed][%u]"), error));
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

// TODO(omaha): needs to handle errors when loading the strings.
CString GetErrorText(HRESULT error, const CString& bundle_name) {
  ASSERT1(!bundle_name.IsEmpty());

  CString error_text;

  switch (error) {
    case GOOPDATE_E_INSTALL_ELEVATED_PROCESS_NEEDS_ELEVATION:
    case GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP:
      error_text.FormatMessage(IDS_NEED_ADMIN_TO_INSTALL, bundle_name);
      break;
    case GOOPDATE_E_ELEVATION_FAILED_ADMIN:
    case GOOPDATE_E_ELEVATION_FAILED_NON_ADMIN:
      error_text.FormatMessage(IDS_ELEVATION_FAILED, bundle_name);
      break;
    case GOOPDATE_E_FAILED_TO_GET_LOCK:
    case GOOPDATE_E_FAILED_TO_GET_LOCK_MATCHING_INSTALL_PROCESS_RUNNING:
    case GOOPDATE_E_FAILED_TO_GET_LOCK_NONMATCHING_INSTALL_PROCESS_RUNNING:
    case GOOPDATE_E_FAILED_TO_GET_LOCK_UPDATE_PROCESS_RUNNING:
      {
        CString company_name;
        VERIFY1(company_name.LoadString(IDS_FRIENDLY_COMPANY_NAME));
        error_text.FormatMessage(IDS_APPLICATION_INSTALLING_GOOGLE_UPDATE,
                                 company_name);
      }
      break;
    case GOOPDATE_E_INSTANCES_RUNNING:
      {
        CString company_name;
        VERIFY1(company_name.LoadString(IDS_FRIENDLY_COMPANY_NAME));
        error_text.FormatMessage(IDS_INSTANCES_RUNNING_AFTER_SHUTDOWN,
                                 company_name,
                                 bundle_name);
      }
      break;
    case GOOPDATE_E_RUNNING_INFERIOR_MSXML:
      error_text.FormatMessage(IDS_WINDOWS_IS_NOT_UP_TO_DATE, bundle_name);
      break;
    case GOOPDATE_E_HANDOFF_FAILED:
      error_text.FormatMessage(IDS_HANDOFF_FAILED, bundle_name);
      break;
    default:
      error_text.FormatMessage(IDS_SETUP_FAILED, error);
      break;
  }

  return error_text;
}

// Displays an error in the Google Update UI and sends a ping if allowed.
void HandleInstallError(HRESULT error,
                        int extra_code1,
                        const CString& session_id,
                        bool is_machine,
                        bool is_interactive,
                        bool is_eula_required,
                        bool is_oem_install,
                        bool is_enterprise_install,
                        const CString& current_version,
                        const CString& install_source,
                        const CommandLineExtraArgs& extra_args,
                        bool has_setup_succeeded,
                        bool has_launched_handoff,
                        bool* has_ui_been_displayed) {
  ASSERT1(FAILED(error));
  ASSERT1(has_ui_been_displayed);

  const CString& bundle_name(extra_args.bundle_name);
  const CString error_text(GetErrorText(error, bundle_name));

  OPT_LOG(LE, (_T("[Failed to install][0x%08x][%s]"), error, error_text));

  if (is_interactive && !*has_ui_been_displayed) {
    CString primary_app_id;
    if (!extra_args.apps.empty()) {
      primary_app_id = GuidToString(extra_args.apps[0].app_guid);
    }
    *has_ui_been_displayed = client_utils::DisplayError(
                                 is_machine,
                                 bundle_name,
                                 error,
                                 extra_code1,
                                 error_text,
                                 primary_app_id,
                                 extra_args.language,
                                 extra_args.installation_id,
                                 extra_args.brand_code);
  }

  // Send an EVENT_INSTALL_COMPLETE ping for Omaha and wait for the ping to be
  // sent. This ping may cause a firewall prompt since the process sending the
  // ping may run from a temporary directory.
  //
  // An EVENT_INSTALL_COMPLETE ping for the apps is also sent in the case the
  /// handoff process did not launch successfully, otherwise, the handoff
  // process is responsible for sending this ping.
  //
  // Setup can fail before setting either eula or oem states in the
  // registry. Therefore, ConfigManager::CanUseNetwork can't be called yet.
  if (is_eula_required || is_oem_install || is_enterprise_install) {
    return;
  }

  Ping ping(is_machine, session_id, install_source);
  ping.LoadAppDataFromExtraArgs(extra_args);
  if (!has_setup_succeeded) {
    PingEventPtr setup_install_complete_ping_event(
        new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,        //  Type 2.
                      PingEvent::EVENT_RESULT_ERROR,
                      error,
                      extra_code1));
    const CString next_version(GetVersionString());
    ping.BuildOmahaPing(current_version,
                        next_version,
                        setup_install_complete_ping_event);
  }
  if (!has_launched_handoff) {
    PingEventPtr install_complete_ping_event(
        new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,         //  Type 2.
                      PingEvent::EVENT_RESULT_ERROR,
                      error,
                      extra_code1));
    ping.BuildAppsPing(install_complete_ping_event);
  }

  HRESULT hr = ping.Send(false);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[ping.Send failed][0x%x]"), hr));
  }
}

}  // namespace internal

// This function handles command-line installs. This is the only function that
// can install Omaha in non-update cases. If elevation is required, the function
// elevates an instance of Omaha and waits for that instance to exit before
// returning. If needed, the function starts a handoff process to install the
// applications and waits for the handoff process to exit.
//
// 'install_cmd_line' parameter is the command line for the current instance and
// used to elevate if necessary.
// 'args' parameter is the parsed args corresponding to install_cmd_line and it
// is used to build the handoff command line if necessary.
HRESULT Install(bool is_interactive,
                bool is_app_install,
                bool is_eula_required,
                bool is_oem_install,
                bool is_enterprise_install,
                bool is_install_elevated_instance,
                const CString& install_cmd_line,
                const CommandLineArgs& args,
                bool* is_machine,
                bool* has_ui_been_displayed) {
  ASSERT1(!install_cmd_line.IsEmpty());
  ASSERT1(is_machine);
  ASSERT1(has_ui_been_displayed);

  CORE_LOG(L2, (_T("[Install][%d][%d][%s]"),
                *is_machine, is_interactive, install_cmd_line));
  ++metric_setup_install_total;
  if (is_install_elevated_instance) {
    ++metric_setup_uac_succeeded;
  }

  if (!*is_machine &&
      vista_util::IsVistaOrLater() &&
      vista_util::IsUserAdmin()) {
    ++metric_setup_user_app_admin;
  }

  // Allocate a session ID to connect all update checks and pings involved
  // with this run of Omaha.  This will need to be passed along to any child
  // processes we create.
  CString session_id;
  VERIFY_SUCCEEDED(GetGuid(&session_id));

  // 'current_version' corresponds to the value of 'pv' read from
  // registry. This value is either empty, in the case of a new install or
  // the version of the installed Omaha before the setup code ran.
  CString current_version;
  app_registry_utils::GetAppVersion(*is_machine,
                                    kGoogleUpdateAppId,
                                    &current_version);

  bool has_setup_succeeded  = false;
  bool has_launched_handoff = false;

  if (IsElevationRequired(*is_machine)) {
    ASSERT1(!ConfigManager::Instance()->IsWindowsInstalling());

    DWORD exit_code = 0;
    HRESULT hr = internal::DoElevation(is_interactive,
                                       is_install_elevated_instance,
                                       install_cmd_line,
                                       &exit_code);

    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[DoElevation failed][%s][0x%08x]"),
                    install_cmd_line, hr));

      bool attempt_per_user_install = false;
      if (is_interactive &&
          args.extra.apps[0].needs_admin == NEEDS_ADMIN_PREFERS) {
        if (hr == GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP ||
            hr == GOOPDATE_E_INSTALL_ELEVATED_PROCESS_NEEDS_ELEVATION) {
          attempt_per_user_install = true;
        } else {
          *has_ui_been_displayed = client_utils::DisplayContinueAsNonAdmin(
              args.extra.bundle_name, &attempt_per_user_install);
        }
      }

      if (attempt_per_user_install) {
        *is_machine = false;
        CString no_admin_cmd_line(String_ReplaceIgnoreCase(install_cmd_line,
                                                           kNeedsAdminPrefers,
                                                           kNeedsAdminNo));
        CommandLineArgs no_admin_args = args;
        no_admin_args.extra.apps[0].needs_admin = NEEDS_ADMIN_NO;
        no_admin_args.extra_args_str = String_ReplaceIgnoreCase(
            no_admin_args.extra_args_str, kNeedsAdminPrefers, kNeedsAdminNo);

        return Install(is_interactive,
                       is_app_install,
                       is_eula_required,
                       is_oem_install,
                       is_enterprise_install,
                       is_install_elevated_instance,
                       no_admin_cmd_line,
                       no_admin_args,
                       is_machine,
                       has_ui_been_displayed);
      }

      internal::HandleInstallError(hr,
                                   0,
                                   session_id,
                                   *is_machine,
                                   is_interactive,
                                   is_eula_required,
                                   is_oem_install,
                                   is_enterprise_install,
                                   current_version,
                                   args.install_source,
                                   args.extra,
                                   has_setup_succeeded,
                                   has_launched_handoff,
                                   has_ui_been_displayed);
      return hr;
    }

    // TODO(omaha): waiting for input idle of an elevated process fails if the
    // caller is not elevated. The code assumes that the elevated process
    // displays UI if the elevation succeeded. It is possible that the elevated
    // process ran but had terminated before displaying UI. This is an uncommon
    // case and for simplicity, it is not handled here.
    *has_ui_been_displayed = true;

    return exit_code;
  }

  SplashScreen splash_screen(args.extra.bundle_name);
  if (is_interactive) {
    splash_screen.Show();
  }

  int extra_code1 = 0;
  HRESULT hr = internal::DoInstall(*is_machine,
                                   is_app_install,
                                   is_eula_required,
                                   is_oem_install,
                                   is_enterprise_install,
                                   current_version,
                                   args,
                                   session_id,
                                   &splash_screen,
                                   &extra_code1,
                                   &has_setup_succeeded,
                                   &has_launched_handoff,
                                   has_ui_been_displayed);
  if (is_interactive) {
    splash_screen.Dismiss();
  }

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DoInstall failed][0x%08x]"), hr));
    internal::HandleInstallError(hr,
                                 extra_code1,
                                 session_id,
                                 *is_machine,
                                 is_interactive,
                                 is_eula_required,
                                 is_oem_install,
                                 is_enterprise_install,
                                 current_version,
                                 args.install_source,
                                 args.extra,
                                 has_setup_succeeded,
                                 has_launched_handoff,
                                 has_ui_been_displayed);
    return hr;
  }

  if (!is_app_install) {
    // TODO(omaha): Display UI if we want to support interactive Omaha-only
    // installs.
    ASSERT1(!is_interactive);
    // TODO(omaha3): Figure out a way to send a ping from the installed location
    // for Omaha-only install success.
  }

  ++metric_setup_install_succeeded;
  return S_OK;
}

HRESULT OemInstall(bool is_interactive,
                   bool is_app_install,
                   bool is_eula_required,
                   bool is_install_elevated_instance,
                   const CString& install_cmd_line,
                   const CommandLineArgs& args,
                   bool* is_machine,
                   bool* has_ui_been_displayed) {
  ASSERT1(is_machine);
  ASSERT1(has_ui_been_displayed);

  // OEM is handled as a special case, and the state must be correct before
  // calling Install().
  HRESULT hr = oem_install_utils::SetOemInstallState(*is_machine);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[SetOemInstallState failed][0x%x]"), hr));
    return hr;
  }

  hr = Install(is_interactive,
               is_app_install,
               is_eula_required,
               true,   // is_oem_install
               false,  // is_enterprise_install
               is_install_elevated_instance,
               install_cmd_line,
               args,
               is_machine,
               has_ui_been_displayed);

  if (FAILED(hr)) {
    VERIFY_SUCCEEDED(oem_install_utils::ResetOemInstallState(*is_machine));
    return hr;
  }

  ASSERT1(oem_install_utils::IsOemInstalling(*is_machine));
  return S_OK;
}

}  // namespace omaha
