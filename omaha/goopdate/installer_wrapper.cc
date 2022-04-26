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

#include <atlpath.h>

#include <vector>

#include "goopdate/omaha3_idl.h"
#include "omaha/goopdate/installer_wrapper.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/const_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/environment_block_modifier.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/app_manager.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"
#include "omaha/goopdate/worker_metrics.h"

namespace omaha {

namespace {

CString BuildMsiCommandLine(const CString& arguments,
                            const CString& msi_file_path,
                            const CString& enclosed_installer_data_file_path) {
  CORE_LOG(L3, (_T("[CreateMsiCommandLine]")));

  CString command_line;
  // Suppressing reboots can lead to an inconsistent state until the user
  // reboots, but automatically rebooting is unacceptable. The user will be
  // informed by the string for ERROR_SUCCESS_REBOOT_REQUIRED that a reboot is
  // necessary. See http://b/1184091 for details.

  if (!enclosed_installer_data_file_path.IsEmpty()) {
    SafeCStringFormat(&command_line, _T("INSTALLERDATA=%s "),
                      enclosed_installer_data_file_path);
  }

  SafeCStringAppendFormat(&command_line, _T("%s %s /qn /i \"%s\""),
                          arguments,
                          kMsiSuppressAllRebootsCmdLine,
                          msi_file_path);

  // The msiexec version in XP SP2 (V 3.01) and higher supports the /log switch.
  if (SystemInfo::IsRunningOnXPSP2OrLater()) {
    CString logfile(msi_file_path);
    logfile.Append(_T(".log"));

    SafeCStringAppendFormat(&command_line, _T(" /log \"%s\""), logfile);
  }

  CORE_LOG(L2, (_T("[msiexec command line][%s]"), command_line));
  return command_line;
}

// Gets the installer exit code.
HRESULT GetInstallerExitCode(const Process& p, uint32* exit_code) {
  ASSERT1(exit_code);

  if (p.Running()) {
    ASSERT(false,
           (_T("GetInstallerExitCode called while the process is running.")));
    return GOOPDATEINSTALL_E_INSTALLER_INTERNAL_ERROR;
  }

  if (!p.GetExitCode(exit_code)) {
    ASSERT(false,
           (_T("[Failed to get the installer exit code for some reason.]")));
    return GOOPDATEINSTALL_E_INSTALLER_INTERNAL_ERROR;
  }

  CORE_LOG(L2, (_T("[Installer exit code][%u]"), *exit_code));

  return S_OK;
}

// Gets the errors string for the specified system error.
// Assumes error_code represents a system error.
void GetSystemErrorString(uint32 error_code,
                          const CString& language,
                          CString* error_string) {
  ASSERT1(error_string);
  ASSERT1(ERROR_SUCCESS != error_code);

  const CString error_code_string = FormatErrorCode(error_code);

  StringFormatter formatter(language);

  const CString error_message(GetMessageForSystemErrorCode(error_code));
  if (!error_message.IsEmpty()) {
    VERIFY_SUCCEEDED(formatter.FormatMessage(error_string,
                                              IDS_INSTALLER_FAILED_WITH_MESSAGE,
                                              error_code_string,
                                              error_message));
  } else {
    VERIFY_SUCCEEDED(formatter.FormatMessage(error_string,
                                              IDS_INSTALLER_FAILED_NO_MESSAGE,
                                              error_code_string));
  }

  OPT_LOG(LEVEL_ERROR, (_T("[installer system error][%u][%s]"),
                        error_code, *error_string));
  ASSERT1(!error_string->IsEmpty());
}

}  // namespace

InstallerWrapper::InstallerWrapper(bool is_machine)
    : is_machine_(is_machine),
      num_tries_when_msi_busy_(1) {
  CORE_LOG(L3, (_T("[InstallerWrapper::InstallerWrapper]")));
}

InstallerWrapper::~InstallerWrapper() {
  CORE_LOG(L3, (_T("[InstallerWrapper::~InstallerWrapper]")));
}

HRESULT InstallerWrapper::Initialize() {
  NamedObjectAttributes lock_attr;
  // TODO(omaha3): We might want to move this lock to the InstallManager.
  GetNamedObjectAttributes(kInstallManagerSerializer, is_machine_, &lock_attr);
  if (!installer_lock_.InitializeWithSecAttr(lock_attr.name, &lock_attr.sa)) {
    OPT_LOG(LEVEL_ERROR, (_T("[Could not init Install Manager lock]")));
    return GOOPDATEINSTALL_E_FAILED_INIT_INSTALLER_LOCK;
  }

  return S_OK;
}

// result_* will be populated if the installer ran and exited, regardless of the
// return value.
// Assumes the call is protected by some mechanism providing exclusive access
// to the app's registry keys in Clients and ClientState.
HRESULT InstallerWrapper::InstallApp(HANDLE user_token,
                                     const GUID& app_guid,
                                     const CString& installer_path,
                                     const CString& arguments,
                                     const CString& installer_data,
                                     const CString& language,
                                     const CString& untrusted_data,
                                     int install_priority,
                                     InstallerResultInfo* result_info) {
  ASSERT1(result_info);

  HRESULT hr = DoInstallApp(user_token,
                            app_guid,
                            installer_path,
                            arguments,
                            installer_data,
                            language,
                            untrusted_data,
                            install_priority,
                            result_info);

  ASSERT1((SUCCEEDED(hr) && result_info->type == INSTALLER_RESULT_SUCCESS) ||
          (GOOPDATEINSTALL_E_INSTALLER_FAILED == hr &&
           (result_info->type == INSTALLER_RESULT_ERROR_MSI ||
            result_info->type == INSTALLER_RESULT_ERROR_SYSTEM ||
            result_info->type == INSTALLER_RESULT_ERROR_OTHER)) ||
          (GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING == hr &&
           (result_info->type == INSTALLER_RESULT_ERROR_MSI ||
            result_info->type == INSTALLER_RESULT_ERROR_SYSTEM)) ||
          (FAILED(hr) && result_info->type == INSTALLER_RESULT_UNKNOWN));
  ASSERT1(!result_info->text.IsEmpty() ==
              (GOOPDATEINSTALL_E_INSTALLER_FAILED == hr ||
               GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT == hr ||
               GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING == hr) ||
          SUCCEEDED(hr));  // Successes may or may not have messages.

  CORE_LOG(L3, (_T("[InstallApp result][0x%x][%s][type: %d][code: %d][%s][%s]"),
                hr, GuidToString(app_guid), result_info->type,
                result_info->code, result_info->text,
                result_info->post_install_launch_command_line));
  return hr;
}

CString InstallerWrapper::GetMessageForError(HRESULT error_code,
                                             const CString& installer_filename,
                                             const CString& language) {
  CString message;
  StringFormatter formatter(language);

  switch (error_code) {
    case GOOPDATEINSTALL_E_FILENAME_INVALID:
      VERIFY_SUCCEEDED(formatter.FormatMessage(&message,
                                                IDS_INVALID_INSTALLER_FILENAME,
                                                installer_filename));
      break;
    case GOOPDATEINSTALL_E_INSTALLER_FAILED_START:
      VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALLER_FAILED_TO_START,
                                             &message));
      break;
    case GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT:
      VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALLER_TIMED_OUT,
                                             &message));
      break;
    case GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY:
    case GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION:
    case GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH:
      VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALL_FAILED, &message));
      break;
    case GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING:
      VERIFY_SUCCEEDED(formatter.LoadString(IDS_MSI_INSTALL_ALREADY_RUNNING,
                                             &message));
      break;
    case GOOPDATEINSTALL_E_INSTALLER_FAILED:
      ASSERT(false,
             (_T("[GetOmahaErrorTextToReport]")
              _T("GOOPDATEINSTALL_E_INSTALLER_FAILED should never be reported ")
              _T("directly. The installer error string should be reported.")));
      VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALL_FAILED, &message));
      break;
    case GOOPDATEINSTALL_E_INSTALLER_INTERNAL_ERROR:
    default:
      ASSERT(false, (_T("[GetOmahaErrorTextToReport]")
                     _T("[An Omaha error occurred that this method does not ")
                     _T("know how to report.][0x%08x]"), error_code));
      VERIFY_SUCCEEDED(formatter.LoadString(IDS_INSTALL_FAILED, &message));
      break;
  }

  ASSERT1(!message.IsEmpty());
  return message;
}

void InstallerWrapper::set_num_tries_when_msi_busy(
    int num_tries_when_msi_busy) {
  ASSERT1(num_tries_when_msi_busy >= 1);
  num_tries_when_msi_busy_ = num_tries_when_msi_busy;
}

HRESULT InstallerWrapper::BuildCommandLineFromFilename(
    const CString& file_path,
    const CString& arguments,
    const CString& installer_data,
    CString* executable_path,
    CString* command_line,
    InstallerType* installer_type) {
  CORE_LOG(L3, (_T("[BuildCommandLineFromFilename]")));

  ASSERT1(executable_path);
  ASSERT1(command_line);
  ASSERT1(installer_type);

  *executable_path = _T("");
  *command_line = _T("");
  *installer_type = UNKNOWN_INSTALLER;

  // The app's installer owns the lifetime of installer data file if it has been
  // created, so Omaha does not delete it.
  CString enclosed_installer_data_file_path;

  // We use the App Installer's directory for the InstallerData file.
  CPath app_installer_directory(file_path);
  if (!app_installer_directory.RemoveFileSpec()) {
    OPT_LOG(LE, (_T("[Does not appear to be a filename '%s']"), file_path));
    return GOOPDATEINSTALL_E_FILENAME_INVALID;
  }

  VERIFY_SUCCEEDED(goopdate_utils::WriteInstallerDataToTempFile(
      app_installer_directory,
      installer_data,
      &enclosed_installer_data_file_path));
  if (!enclosed_installer_data_file_path.IsEmpty()) {
    EnclosePath(&enclosed_installer_data_file_path);
  }

  // PathFindExtension returns the address of the trailing NUL character if an
  // extension is not found. It does not return NULL.
  const TCHAR* ext = ::PathFindExtension(file_path);
  ASSERT1(ext);
  if (*ext != _T('\0')) {
    ext++;  // Skip the period.
    if (0 == lstrcmpi(ext, _T("exe"))) {
      *executable_path = file_path;
      if (enclosed_installer_data_file_path.IsEmpty()) {
        *command_line = arguments;
      } else {
        SafeCStringFormat(command_line, _T("%s %s%s"),
                          arguments,
                          kCmdLineInstallerData,
                          enclosed_installer_data_file_path);
      }
      *installer_type = CUSTOM_INSTALLER;

      CORE_LOG(L2, (_T("[BuildCommandLineFromFilename][exe][%s][%s]"),
                    *executable_path, *command_line));
    } else if (0 == lstrcmpi(ext, _T("msi"))) {
      *executable_path = _T("msiexec");
      *command_line = BuildMsiCommandLine(arguments,
                                          file_path,
                                          enclosed_installer_data_file_path);
      *installer_type = MSI_INSTALLER;

      CORE_LOG(L2, (_T("[BuildCommandLineFromFilename][msi][%s]"),
                    *command_line));
    } else {
      *executable_path = _T("");
      *command_line = _T("");
      *installer_type = UNKNOWN_INSTALLER;

      OPT_LOG(LE, (_T("[Unsupported extension '%s' in %s]"), ext, file_path));
      return GOOPDATEINSTALL_E_FILENAME_INVALID;
    }
  } else {
    OPT_LOG(LE, (_T("[No extension found in %s]"), file_path));
    return GOOPDATEINSTALL_E_FILENAME_INVALID;
  }

  return S_OK;
}

// Calls DoExecuteAndWaitForInstaller to do the work. If an MSI installer
// returns, ERROR_INSTALL_ALREADY_RUNNING waits and retries several times or
// until the installation succeeds.
HRESULT InstallerWrapper::ExecuteAndWaitForInstaller(
    HANDLE user_token,
    const GUID& app_guid,
    const CString& executable_path,
    const CString& command_line,
    InstallerType installer_type,
    const CString& language,
    const CString& untrusted_data,
    int install_priority,
    InstallerResultInfo* result_info) {
  CORE_LOG(L3, (_T("[InstallerWrapper::ExecuteAndWaitForInstaller]")));
  ASSERT1(result_info);
  ASSERT1(num_tries_when_msi_busy_ >= 1);

  ++metric_worker_install_execute_total;
  if (MSI_INSTALLER == installer_type) {
    ++metric_worker_install_execute_msi_total;
  }

  // Run the installer, retrying if necessary.
  int retry_delay = kMsiAlreadyRunningRetryDelayBaseMs;
  int num_tries(0);
  HRESULT hr = GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING;
  for (num_tries = 0;
       hr == GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING &&
       num_tries < num_tries_when_msi_busy_;
       ++num_tries) {
    // Reset the result info - it contains the previous error when retrying.
    *result_info = InstallerResultInfo();

    if (0 < num_tries) {
      // Retrying - wait between attempts.
      CORE_LOG(L1, (_T("[Retrying][%d]"), num_tries));
      ::Sleep(retry_delay);
      retry_delay *= 2;  // Double the retry delay next time.
    }

    hr = DoExecuteAndWaitForInstaller(user_token,
                                      app_guid,
                                      executable_path,
                                      command_line,
                                      installer_type,
                                      language,
                                      untrusted_data,
                                      install_priority,
                                      result_info);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[DoExecuteAndWaitForInstaller failed][0x%08x]"), hr));
      return hr;
    }
    CORE_LOG(L1, (_T("[Installer result][%d][%d][%s]"),
                  result_info->type, result_info->code, result_info->text));
    ASSERT1(result_info->type != INSTALLER_RESULT_UNKNOWN);

    if ((INSTALLER_RESULT_ERROR_MSI == result_info->type ||
         INSTALLER_RESULT_ERROR_SYSTEM == result_info->type) &&
        ERROR_INSTALL_ALREADY_RUNNING == result_info->code) {
      hr = GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING;
    }
  }

  if (1 < num_tries) {
    // Record metrics about the ERROR_INSTALL_ALREADY_RUNNING retries.
// TODO(omaha3): If we're willing to have a single metric for installs and
// updates, we can avoid knowing is_update.
#if 0
    if (!app_version_->is_update()) {
#endif
      ++metric_worker_install_msi_in_progress_detected_install;
      if (result_info->type == INSTALLER_RESULT_SUCCESS) {
        ++metric_worker_install_msi_in_progress_retry_succeeded_install;
        metric_worker_install_msi_in_progress_retry_succeeded_tries_install
            = num_tries;
      }
#if 0
    } else {
      ++metric_worker_install_msi_in_progress_detected_update;
      if (result_info->type == INSTALLER_RESULT_SUCCESS) {
        ++metric_worker_install_msi_in_progress_retry_succeeded_update;
        metric_worker_install_msi_in_progress_retry_succeeded_tries_update
            = num_tries;
      }
    }
#endif
  }

  return hr;
}

HRESULT InstallerWrapper::DoExecuteAndWaitForInstaller(
    HANDLE user_token,
    const GUID& app_guid,
    const CString& executable_path,
    const CString& command_line,
    InstallerType installer_type,
    const CString& language,
    const CString& untrusted_data,
    int install_priority,
    InstallerResultInfo* result_info) {
  OPT_LOG(L1, (_T("[Running installer][%s][%s][%s]"),
               executable_path, command_line, GuidToString(app_guid)));
  ASSERT1(result_info);

  AppManager::Instance()->ClearInstallerResultApiValues(app_guid);

  // Create modified environment block to pass untrusted data.
  std::vector<TCHAR> env_block;
  EnvironmentBlockModifier eb_mod;
  if (!untrusted_data.IsEmpty()) {
    eb_mod.SetVar(kEnvVariableUntrustedData, untrusted_data);
  }
  eb_mod.SetVar(kEnvVariableIsMachine, is_machine_ ? _T("1") : _T("0"));

  bool eb_res = user_token ?
                    eb_mod.CreateForUser(user_token, &env_block) :
                    eb_mod.CreateForCurrentUser(&env_block);
  if (!eb_res) {
    HRESULT hr = HRESULTFromLastError();
    ASSERT(false, (_T("[EnvironmentBlockModifier failed][0x%x]"), hr));
    return hr;
  }

  ASSERT1(!env_block.empty());

  Process p(executable_path, NULL);
  HRESULT hr = p.StartWithEnvironment(command_line,
                                      user_token,
                                      &env_block.front());
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[p.Start fail][0x%08x][%s][%s]"),
        hr, executable_path, command_line));
    set_error_extra_code1(static_cast<int>(hr));
    return GOOPDATEINSTALL_E_INSTALLER_FAILED_START;
  }

  if (install_priority != INSTALL_PRIORITY_HIGH) {
    VERIFY1(::SetPriorityClass(p.GetHandle(), BELOW_NORMAL_PRIORITY_CLASS));
  }

  // TODO(omaha): InstallerWrapper should not special case Omaha. It is better
  // to have an abstraction such as waiting or not for the installer to
  // exit and let the App state machine special case Omaha. It's too low level
  // to make a decision like this in the InstallerWrapper. Same for all
  // kinds of tests on the call stack above this call that the app_guid is
  // Omaha's guid.
  if (::IsEqualGUID(app_guid, kGoopdateGuid)) {
    // Do not wait for the installer when installing Omaha.
    result_info->type = INSTALLER_RESULT_SUCCESS;
    return S_OK;
  }

  p.WaitUntilDead(kInstallerCompleteIntervalMs);
  hr = GetInstallerResult(app_guid, installer_type, p, language, result_info);

  if (result_info->type != INSTALLER_RESULT_SUCCESS) {
    OPT_LOG(LE, (_T("[Installer failed][%s][%s][%u]"),
                 executable_path, command_line, result_info->code));
  }

  return hr;
}

HRESULT InstallerWrapper::GetInstallerResult(const GUID& app_guid,
                                             InstallerType installer_type,
                                             const Process& p,
                                             const CString& language,
                                             InstallerResultInfo* result_info) {
  CORE_LOG(L3, (_T("[InstallerWrapper::GetInstallerResult]")));
  ASSERT1(result_info);

  HRESULT hr = S_OK;
  uint32 exit_code = 0;
  if (p.Running()) {
    OPT_LOG(LEVEL_WARNING, (_T("[Installer has timed out]")));
    hr = GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT;
    exit_code = static_cast<uint32>(hr);
  } else {
    hr = GetInstallerExitCode(p, &exit_code);
    if (FAILED(hr)) {
      CORE_LOG(LEVEL_ERROR, (_T("[GetInstallerExitCode failed][0x%08x]"), hr));
      return hr;
    }
    hr = S_OK;
  }

  GetInstallerResultHelper(app_guid,
                           installer_type,
                           exit_code,
                           language,
                           result_info);
  return hr;
}

// The default InstallerResult behavior can be overridden in the registry.
// By default, error_code is the exit code. For some InstallerResults, it
// can be overridden by InstallerError in the registry.
// The success string cannot be overridden.
void InstallerWrapper::GetInstallerResultHelper(
    const GUID& app_guid,
    InstallerType installer_type,
    uint32 result_code,
    const CString& language,
    InstallerResultInfo* result_info) {
  ASSERT1(result_info);

  AppManager::InstallerResult installer_result =
      AppManager::INSTALLER_RESULT_DEFAULT;
  InstallerResultInfo result;

  result.code = result_code;

  AppManager& app_manager = *AppManager::Instance();
  app_manager.ReadInstallerResultApiValues(
      app_guid,
      &installer_result,
      &result.code,
      &result.extra_code1,
      &result.text,
      &result.post_install_launch_command_line);
  OPT_LOG(L1, (_T("[InstallerResult][%s][%u]"),
               GuidToString(app_guid), installer_result));

  switch (installer_result) {
    case AppManager::INSTALLER_RESULT_SUCCESS:
      result.type = INSTALLER_RESULT_SUCCESS;
      // TODO(omaha3): Support custom success messages.
      break;
    case AppManager::INSTALLER_RESULT_FAILED_CUSTOM_ERROR:
      result.type = INSTALLER_RESULT_ERROR_OTHER;
      break;
    case AppManager::INSTALLER_RESULT_FAILED_MSI_ERROR:
      result.type = INSTALLER_RESULT_ERROR_MSI;
      break;
    case AppManager::INSTALLER_RESULT_FAILED_SYSTEM_ERROR:
      result.type = INSTALLER_RESULT_ERROR_SYSTEM;
      break;
    case AppManager::INSTALLER_RESULT_EXIT_CODE:
      ASSERT(result.code == result_code, (_T("InstallerError overridden")));
      if (0 == result_code) {
        result.type = INSTALLER_RESULT_SUCCESS;
        result.code = 0;
      } else if (GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT == result_code) {
        result.type = INSTALLER_RESULT_UNKNOWN;
      } else {
        switch (installer_type) {
          case MSI_INSTALLER:
            result.type = INSTALLER_RESULT_ERROR_MSI;
            break;
          case UNKNOWN_INSTALLER:
          case CUSTOM_INSTALLER:
          case MAX_INSTALLER:
          default:
            result.type = INSTALLER_RESULT_ERROR_OTHER;
            break;
        }
      }
      break;
    case AppManager::INSTALLER_RESULT_MAX:
    default:
      ASSERT1(false);
      break;
  }

  // Handle the reboot required case.
  if ((INSTALLER_RESULT_ERROR_MSI == result.type ||
       INSTALLER_RESULT_ERROR_SYSTEM == result.type) &&
      (ERROR_SUCCESS_REBOOT_REQUIRED == result.code)) {
    // Reboot takes precedence over other actions.
    result.type = INSTALLER_RESULT_SUCCESS;
    result.code = 0;
    result.post_install_action = POST_INSTALL_ACTION_REBOOT;
  } else if (!result.post_install_launch_command_line.IsEmpty()) {
    result.post_install_action = POST_INSTALL_ACTION_LAUNCH_COMMAND;
  }

  // InstallerResultInfo status has been finalized. Make sure all errors
  // have error strings.
  switch (result.type) {
    case INSTALLER_RESULT_SUCCESS:
      break;

    case INSTALLER_RESULT_ERROR_MSI:
    case INSTALLER_RESULT_ERROR_SYSTEM:
      GetSystemErrorString(result.code, language, &result.text);
      break;

    case INSTALLER_RESULT_ERROR_OTHER:
      if (result.text.IsEmpty()) {
        result.text.FormatMessage(IDS_INSTALLER_FAILED_NO_MESSAGE,
                                  FormatErrorCode(result.code));
      }
      break;

    case INSTALLER_RESULT_UNKNOWN:
      if (result.text.IsEmpty()) {
        result.text = GetMessageForError(result.code, NULL, language);
      }
      break;
    default:
      ASSERT1(false);
  }

  // TODO(omaha3): Serialize InstallerResultInfo.
  // OPT_LOG(L1, (_T("[%s]"), result.ToString()));
  *result_info = result;
}

// TODO(omaha3): Consider moving this method out of this class, maybe into
// InstallManager.
HRESULT InstallerWrapper::CheckApplicationRegistration(
    const GUID& app_guid,
    const CString& registered_version,
    const CString& expected_version,
    const CString& previous_version,
    bool is_update) const {
  const CString app_guid_string = GuidToString(app_guid);
  CORE_LOG(L2, (_T("[InstallerWrapper::CheckApplicationRegistration][%s]"),
                app_guid_string));
  ASSERT(!::IsEqualGUID(kGoopdateGuid, app_guid),
         (_T("Probably do not want to call this method for Omaha")));

  if (registered_version.IsEmpty()) {
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENTS_KEY;
  }

  CORE_LOG(L2, (_T("[CheckApplicationRegistration]")
                _T("[guid=%s][registered=%s][expected=%s][previous=%s]"),
                app_guid_string, registered_version, expected_version,
                previous_version));

  if (!expected_version.IsEmpty() && registered_version != expected_version) {
    OPT_LOG(LE, (_T("[Registered version does not match expected][%s][%s][%s]"),
                 app_guid_string, registered_version, expected_version));
    // This is expected if a newer version is already installed. Do not fail
    // here in that case. This only works for four-element version strings.
    // If the version format is not recognized, VersionFromString() returns 0.

    ULONGLONG registered_version_number = VersionFromString(registered_version);
    ULONGLONG expected_version_number = VersionFromString(expected_version);

    // Check that the version did not change, the registered version is newer,
    // and neither VersionFromString() call failed.
    if (is_update && registered_version != previous_version ||
        registered_version_number < expected_version_number ||
        !registered_version_number ||
        !expected_version_number) {
      return GOOPDATEINSTALL_E_INSTALLER_VERSION_MISMATCH;
    }

    CORE_LOG(L1, (_T("[Newer version already registered]")));
  }

  if (is_update && previous_version == registered_version) {
    ASSERT1(!previous_version.IsEmpty());
    ASSERT(expected_version.IsEmpty() ||
           VersionFromString(expected_version) >
               VersionFromString(previous_version) ||
           VersionFromString(expected_version) == 0 ||
           VersionFromString(previous_version) == 0,
           (_T("expected_version should be > previous_version when ")
            _T("is_update - possibly a bad update rule.")));

    OPT_LOG(LE, (_T("[Installer did not change version][%s][%s]"),
                 app_guid_string, previous_version));
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION;
  }

  return S_OK;
}

// Assumes installer_lock_ has been initialized.
HRESULT InstallerWrapper::DoInstallApp(HANDLE user_token,
                                       const GUID& app_guid,
                                       const CString& installer_path,
                                       const CString& arguments,
                                       const CString& installer_data,
                                       const CString& language,
                                       const CString& untrusted_data,
                                       int install_priority,
                                       InstallerResultInfo* result_info) {
  CORE_LOG(L1, (_T("[InstallerWrapper::DoInstallApp][%s][%s][%s]"),
               GuidToString(app_guid), installer_path, arguments));
  ASSERT1(result_info);

  CString executable_path;
  CString command_line;
  InstallerType installer_type = UNKNOWN_INSTALLER;

  // TODO(omaha): Remove when http://b/1443404 is addressed.
  const TCHAR* const kChromePerMachineArg = _T("--system-level");
  CString modified_arguments = arguments;
  if (kChromeAppId == GuidToString(app_guid) && is_machine_) {
    SafeCStringAppendFormat(&modified_arguments, _T(" %s"),
                            kChromePerMachineArg);
  }

  HRESULT hr = BuildCommandLineFromFilename(installer_path,
                                            modified_arguments,
                                            installer_data,
                                            &executable_path,
                                            &command_line,
                                            &installer_type);

  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[BuildCommandLineFromFilename failed][0x%08x]"), hr));
    ASSERT1(GOOPDATEINSTALL_E_FILENAME_INVALID == hr);
    return hr;
  }

  // Acquire the global lock here. This will ensure that we are the only
  // installer running of the multiple goopdates.
  __mutexBlock(installer_lock_) {
    hr = ExecuteAndWaitForInstaller(user_token,
                                    app_guid,
                                    executable_path,
                                    command_line,
                                    installer_type,
                                    language,
                                    untrusted_data,
                                    install_priority,
                                    result_info);
  }

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ExecuteAndWaitForInstaller failed][0x%08x][%s]"),
                  hr, GuidToString(app_guid)));
    return hr;
  }

  if (result_info->type != INSTALLER_RESULT_SUCCESS) {
    CORE_LOG(LE, (_T("[Installer failed][%d]"), result_info->type));
    return GOOPDATEINSTALL_E_INSTALLER_FAILED;
  }

  return S_OK;
}

}  // namespace omaha
