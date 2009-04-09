// Copyright 2007-2009 Google Inc.
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

#include "omaha/worker/install_manager.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/process.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/system_info.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource.h"
#include "omaha/worker/job.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

bool GetMessageForSystemErrorCode(DWORD system_error_code, CString* message);

namespace {

// This is the base retry delay between retries when msiexec returns
// ERROR_INSTALL_ALREADY_RUNNING. We exponentially backoff from this value.
// Note that there is an additional delay for the MSI call, so the tries may
// be a few seconds further apart.
int kMsiAlreadyRunningRetryDelayBaseMs = 5000;
// Number of retries. Updates are silent so we can wait longer.
int kNumMsiAlreadyRunningInteractiveMaxTries = 4;  // Up to 35 seconds.
int kNumMsiAlreadyRunningSilentMaxTries      = 7;  // Up to 6.25 minutes.

// Interval to wait for installer completion.
const int kInstallManagerCompleteIntervalMs = 15 * 60 * 1000;

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

// Obtains the localized text for Omaha errors that may occur in the
// DoInstallation path.
void GetOmahaErrorTextToReport(HRESULT omaha_hr,
                               const CString& installer_filename,
                               const CString& app_name,
                               CString* error_text) {
  ASSERT1(error_text);

  switch (omaha_hr) {
    case GOOPDATEINSTALL_E_FILENAME_INVALID:
      error_text->FormatMessage(IDS_INVALID_INSTALLER_FILENAME,
                                installer_filename);
      break;
    case GOOPDATEINSTALL_E_INSTALLER_FAILED_START:
      error_text->FormatMessage(IDS_INSTALLER_FAILED_TO_START);
      break;
    case GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT:
      error_text->FormatMessage(IDS_INSTALLER_TIMED_OUT, app_name);
      break;
    case GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY:
    case GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION:
    case GOOPDATE_E_INVALID_INSTALL_DATA_INDEX:
    case GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS:
      error_text->FormatMessage(IDS_INSTALL_FAILED, omaha_hr);
      break;
    case GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING:
      error_text->FormatMessage(IDS_MSI_INSTALL_ALREADY_RUNNING, app_name);
      break;
    case GOOPDATEINSTALL_E_INSTALLER_FAILED:
      ASSERT(false,
             (_T("[GetOmahaErrorTextToReport]")
              _T("GOOPDATEINSTALL_E_INSTALLER_FAILED should never be reported ")
              _T("directly. The installer error string should be reported.")));
      error_text->FormatMessage(IDS_INSTALL_FAILED, omaha_hr);
      break;
    case GOOPDATEINSTALL_E_INSTALLER_INTERNAL_ERROR:
    default:
      ASSERT(false, (_T("[GetOmahaErrorTextToReport]")
                     _T("[An Omaha error occurred that this method does not ")
                     _T("know how to report.][0x%08x]"), omaha_hr));

      error_text->FormatMessage(IDS_INSTALL_FAILED, omaha_hr);
      break;
  }

  ASSERT1(!error_text->IsEmpty());
}

// Gets the errors string for the specified system error.
// Assumes error_code represents a system error.
void GetSystemErrorString(uint32 error_code, CString* error_string) {
  ASSERT1(error_string);
  ASSERT1(ERROR_SUCCESS != error_code);

  const CString error_code_string = FormatErrorCode(error_code);

  CString error_message;
  if (GetMessageForSystemErrorCode(error_code, &error_message)) {
    ASSERT(!error_message.IsEmpty(),
           (_T("[GetMessageForSystemErrorCode succeeded ")
            _T("but the error message is empty.]")));

    error_string->FormatMessage(IDS_INSTALLER_FAILED_WITH_MESSAGE,
                                error_code_string,
                                error_message);
  } else {
    error_string->FormatMessage(IDS_INSTALLER_FAILED_NO_MESSAGE,
                                error_code_string);
  }

  OPT_LOG(LEVEL_ERROR, (_T("[installer system error][%u][%s]"),
                        error_code, *error_string));
  ASSERT1(!error_string->IsEmpty());
}

}  // namespace

// Obtains the message for a System Error Code in the user's language.
// Returns whether the message was successfully obtained.
// It does not support "insert sequences". The sequence will be returned in the
// string.
bool GetMessageForSystemErrorCode(DWORD system_error_code, CString* message) {
  CORE_LOG(L3, (_T("[GetMessageForSystemErrorCode][%u]"), system_error_code));

  ASSERT1(message);

  message->Empty();

  TCHAR* system_allocated_buffer = NULL;
  const DWORD kFormatOptions = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS |
                               FORMAT_MESSAGE_MAX_WIDTH_MASK;
  DWORD tchars_written = ::FormatMessage(
      kFormatOptions,
      NULL,
      system_error_code,
      0,
      reinterpret_cast<LPWSTR>(&system_allocated_buffer),
      0,
      NULL);

  if (0 < tchars_written) {
    ASSERT1(system_allocated_buffer);
    *message = system_allocated_buffer;

    VERIFY1(!::LocalFree(system_allocated_buffer));
    return true;
  } else {
    DWORD format_message_error = ::GetLastError();
    ASSERT(false, (_T("[::FormatMessage failed][%u]"), format_message_error));
    ASSERT1(!system_allocated_buffer);

    return false;
  }
}

InstallManager::InstallManager(bool is_machine)
    : is_machine_(is_machine) {
}

HRESULT InstallManager::InstallJob(Job* job) {
  ASSERT1(job);
  job_ = job;

  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kInstallManagerSerializer, is_machine_, &lock_attr);
  if (!installer_lock_.InitializeWithSecAttr(lock_attr.name, &lock_attr.sa)) {
    OPT_LOG(LEVEL_ERROR, (_T("[Could not init install manager lock]")));
    HRESULT hr = GOOPDATEINSTALL_E_CANNOT_GET_INSTALLER_LOCK;
    error_info_.status = COMPLETION_ERROR;
    error_info_.error_code = hr;
    error_info_.text.FormatMessage(IDS_INSTALL_FAILED, hr);
    return hr;
  }

  HRESULT hr = InstallDownloadedFile();

  // error_info_ should have the default values unless the installer failed in
  // which case it is already popluated correctly.
  ASSERT1(IsCompletionSuccess(error_info_) ||
          (GOOPDATEINSTALL_E_INSTALLER_FAILED == hr &&
           IsCompletionInstallerError(error_info_)) ||
          (GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING == hr &&
           (COMPLETION_INSTALLER_ERROR_MSI == error_info_.status ||
            COMPLETION_INSTALLER_ERROR_SYSTEM == error_info_.status)));
  ASSERT1(!error_info_.text.IsEmpty() ==
          (GOOPDATEINSTALL_E_INSTALLER_FAILED == hr ||
          GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING == hr));

  if (SUCCEEDED(hr)) {
    // Omaha and the installer succeeded.
    error_info_.text.FormatMessage(IDS_APPLICATION_INSTALLED_SUCCESSFULLY,
                                   job_->app_data().display_name());
    return S_OK;
  } else {
    CORE_LOG(LEVEL_ERROR, (_T("[InstallDownloadedFile failed][0x%08x]"), hr));

    if (GOOPDATEINSTALL_E_INSTALLER_FAILED != hr) {
      // Omaha failed.
      error_info_.status = COMPLETION_ERROR;
      error_info_.error_code = hr;
      GetOmahaErrorTextToReport(hr,
                                job_->download_file_name(),
                                job_->app_data().display_name(),
                                &error_info_.text);
    }

    return hr;
  }
}

HRESULT InstallManager::BuildCommandLineFromFilename(
    const CString& filename,
    const CString& arguments,
    const CString& installer_data,
    CString* executable_name,
    CString* command_line,
    InstallerType* installer_type) {
  CORE_LOG(L3, (_T("[BuildCommandLineFromFilename]")));

  ASSERT1(executable_name);
  ASSERT1(command_line);
  ASSERT1(installer_type);

  *executable_name = _T("");
  *command_line = _T("");
  *installer_type = UNKNOWN_INSTALLER;

  // The app's installer owns the lifetime of installer data file if it has been
  // created, so Omaha does not delete it.
  CString enclosed_installer_data_file_path;

  VERIFY1(SUCCEEDED(goopdate_utils::WriteInstallerDataToTempFile(
      installer_data,
      &enclosed_installer_data_file_path)));
  if (!enclosed_installer_data_file_path.IsEmpty()) {
    EnclosePath(&enclosed_installer_data_file_path);
  }

  // PathFindExtension returns the address of the trailing NUL character if an
  // extension is not found. It does not return NULL.
  const TCHAR* ext = ::PathFindExtension(filename);
  ASSERT1(ext);
  if (*ext != _T('\0')) {
    ext++;  // Skip the period.
    if (0 == lstrcmpi(ext, _T("exe"))) {
      *executable_name = filename;
      if (enclosed_installer_data_file_path.IsEmpty()) {
        *command_line = arguments;
      } else {
        command_line->Format(_T("%s /installerdata=%s"),
                             arguments,
                             enclosed_installer_data_file_path);
      }
      *installer_type = CUSTOM_INSTALLER;

      CORE_LOG(L2, (_T("[BuildCommandLineFromFilename][exe][%s][%s]"),
                    *executable_name, *command_line));
    } else if (0 == lstrcmpi(ext, _T("msi"))) {
      *executable_name = _T("msiexec");
      *command_line = BuildMsiCommandLine(arguments,
                                          filename,
                                          enclosed_installer_data_file_path);
      *installer_type = MSI_INSTALLER;

      CORE_LOG(L2, (_T("[BuildCommandLineFromFilename][msi][%s]"),
                    *command_line));
    } else {
      *executable_name = _T("");
      *command_line = _T("");
      *installer_type = UNKNOWN_INSTALLER;

      OPT_LOG(LEVEL_ERROR, (_T("[Unsupported extension '%s' in %s]"),
                            ext, filename));
      return GOOPDATEINSTALL_E_FILENAME_INVALID;
    }
  } else {
    OPT_LOG(LEVEL_ERROR, (_T("[No extension found in %s]"), filename));
    return GOOPDATEINSTALL_E_FILENAME_INVALID;
  }

  return S_OK;
}

// Calls DoExecuteAndWaitForInstaller to do the work. If an MSI installer
// returns, ERROR_INSTALL_ALREADY_RUNNING waits and retries several times or
// until the installation succeeds.
HRESULT InstallManager::ExecuteAndWaitForInstaller(
    const CString& executable_name,
    const CString& command_line,
    const CString& app_guid,
    InstallerType installer_type) {
  CORE_LOG(L3, (_T("[InstallManager::ExecuteAndWaitForInstaller]")));

  ++metric_worker_install_execute_total;
  if (MSI_INSTALLER == installer_type) {
    ++metric_worker_install_execute_msi_total;
  }

  // Run the installer, retrying if necessary.
  const int max_tries = job_->is_background() ?
                        kNumMsiAlreadyRunningSilentMaxTries :
                        kNumMsiAlreadyRunningInteractiveMaxTries;
  int retry_delay = kMsiAlreadyRunningRetryDelayBaseMs;
  int num_tries(0);
  bool retry(true);
  for (num_tries = 0; retry && num_tries < max_tries; ++num_tries) {
    // Reset the completion info - it contains the previous error when retrying.
    error_info_ = CompletionInfo();

    if (0 < num_tries) {
      // Retrying - wait between attempts.
      ASSERT1(MSI_INSTALLER == installer_type);
      ::Sleep(retry_delay);
      retry_delay *= 2;  // Double the retry delay next time.
    }

    HRESULT hr = DoExecuteAndWaitForInstaller(executable_name,
                                              command_line,
                                              app_guid,
                                              installer_type);
    if (FAILED(hr)) {
      return hr;
    }

    retry = (COMPLETION_INSTALLER_ERROR_MSI == error_info_.status ||
             COMPLETION_INSTALLER_ERROR_SYSTEM == error_info_.status) &&
            ERROR_INSTALL_ALREADY_RUNNING == error_info_.error_code;
  }

  if (1 < num_tries) {
    // Record metrics about the ERROR_INSTALL_ALREADY_RUNNING retries.
    ASSERT1(MSI_INSTALLER == installer_type);

    if (!job_->is_update()) {
      ++metric_worker_install_msi_in_progress_detected_install;
      if (IsCompletionSuccess(error_info_)) {
        ++metric_worker_install_msi_in_progress_retry_succeeded_install;
        metric_worker_install_msi_in_progress_retry_succeeded_tries_install
            = num_tries;
      }
    } else {
      ++metric_worker_install_msi_in_progress_detected_update;
      if (IsCompletionSuccess(error_info_)) {
        ++metric_worker_install_msi_in_progress_retry_succeeded_update;
        metric_worker_install_msi_in_progress_retry_succeeded_tries_update
            = num_tries;
      }
    }
  }

  if ((COMPLETION_INSTALLER_ERROR_MSI == error_info_.status ||
       COMPLETION_INSTALLER_ERROR_SYSTEM == error_info_.status) &&
      ERROR_INSTALL_ALREADY_RUNNING == error_info_.error_code) {
    return GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING;
  }

  if (IsCompletionInstallerError(error_info_)) {
    return GOOPDATEINSTALL_E_INSTALLER_FAILED;
  }

  return S_OK;
}

HRESULT InstallManager::DoExecuteAndWaitForInstaller(
    const CString& executable_name,
    const CString& command_line,
    const CString& app_guid,
    InstallerType installer_type) {
  OPT_LOG(L1, (_T("[Running installer]")
               _T("[%s][%s][%s]"), executable_name, command_line, app_guid));

  CleanupInstallerResultRegistry(app_guid);

  Process p(executable_name, NULL);

  if (!p.Start(command_line)) {
    OPT_LOG(LEVEL_ERROR, (_T("[Could not start process]")
                          _T("[%s][%s]"), executable_name, command_line));
    return GOOPDATEINSTALL_E_INSTALLER_FAILED_START;
  }

  if (app_guid.CompareNoCase(kGoogleUpdateAppId) == 0) {
    // Do not wait for the installer when installing Omaha.
    return S_OK;
  }

  if (!p.WaitUntilDead(kInstallManagerCompleteIntervalMs)) {
    OPT_LOG(LEVEL_WARNING, (_T("[Installer has timed out]")
                            _T("[%s][%s]"), executable_name, command_line));
    return GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT;
  }

  HRESULT hr = GetInstallerResult(app_guid, installer_type, p, &error_info_);
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[GetInstallerResult failed][0x%08x]"), hr));
    return hr;
  }

  if (IsCompletionInstallerError(error_info_)) {
    OPT_LOG(LE, (_T("[Installer failed][%s][%s][%u]"),
                 executable_name, command_line, error_info_.error_code));
  }

  return S_OK;
}

CString InstallManager::BuildMsiCommandLine(
    const CString& arguments,
    const CString& filename,
    const CString& enclosed_installer_data_file_path) {
  CORE_LOG(L3, (_T("[InstallManager::CreateMsiCommandLine]")));

  CString command_line;
  // Suppressing reboots can lead to an inconsistent state until the user
  // reboots, but automatically rebooting is unacceptable. The user will be
  // informed by the string for ERROR_SUCCESS_REBOOT_REQUIRED that a reboot is
  // necessary. See http://b/1184091 for details.

  if (!enclosed_installer_data_file_path.IsEmpty()) {
    command_line.Format(_T("INSTALLERDATA=%s "),
                        enclosed_installer_data_file_path);
  }

  command_line.AppendFormat(_T("%s REBOOT=ReallySuppress /qn /i \"%s\""),
                            arguments, filename);

  // The msiexec version in XP SP2 (V 3.01) and higher supports the /log switch.
  if (SystemInfo::OSWinXPSP2OrLater()) {
    CString logfile(filename);
    logfile.Append(_T(".log"));

    command_line.AppendFormat(_T(" /log \"%s\""), logfile);
  }

  CORE_LOG(L2, (_T("[msiexec command line][%s]"), command_line));
  return command_line;
}

HRESULT InstallManager::GetInstallerResult(const CString& app_guid,
                                           InstallerType installer_type,
                                           const Process& p,
                                           CompletionInfo* completion_info) {
  CORE_LOG(L3, (_T("[InstallManager::GetInstallerResult]")));
  ASSERT1(completion_info);

  uint32 exit_code = 0;
  HRESULT hr = GetInstallerExitCode(p, &exit_code);
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[GetInstallerExitCode failed][0x%08x]"), hr));
    return hr;
  }

  GetInstallerResultHelper(app_guid,
                           installer_type,
                           exit_code,
                           completion_info);
  return S_OK;
}

// The default InstallerResult behavior can be overridden in the registry.
// By default, error_code is the exit code. For some InstallerResults, it
// can be overridden by InstallerError in the registry.
// The success string cannot be overridden.
void InstallManager::GetInstallerResultHelper(const CString& app_guid,
                                              InstallerType installer_type,
                                              uint32 exit_code,
                                              CompletionInfo* completion_info) {
  completion_info->error_code = exit_code;
  completion_info->text.Empty();

  const CString app_client_state_key =
      goopdate_utils::GetAppClientStateKey(is_machine_, app_guid);
  const InstallerResult installer_result =
      GetInstallerResultType(app_client_state_key);
  OPT_LOG(L1, (_T("[InstallerResult][%s][%u]"), app_guid, installer_result));


  switch (installer_result) {
    case INSTALLER_RESULT_SUCCESS:
      completion_info->status = COMPLETION_SUCCESS;
      ReadInstallerErrorOverride(app_client_state_key,
                                 &completion_info->error_code);
      break;
    case INSTALLER_RESULT_FAILED_CUSTOM_ERROR:
      completion_info->status = COMPLETION_INSTALLER_ERROR_OTHER;
      ReadInstallerErrorOverride(app_client_state_key,
                                 &completion_info->error_code);
      ReadInstallerResultUIStringOverride(app_client_state_key,
                                          &completion_info->text);
      break;
    case INSTALLER_RESULT_FAILED_MSI_ERROR:
      completion_info->status = COMPLETION_INSTALLER_ERROR_MSI;
      ReadInstallerErrorOverride(app_client_state_key,
                                 &completion_info->error_code);
      break;
    case INSTALLER_RESULT_FAILED_SYSTEM_ERROR:
      completion_info->status = COMPLETION_INSTALLER_ERROR_SYSTEM;
      ReadInstallerErrorOverride(app_client_state_key,
                                 &completion_info->error_code);
      break;
    case INSTALLER_RESULT_EXIT_CODE:
      if (0 == exit_code) {
        completion_info->status = COMPLETION_SUCCESS;
        completion_info->error_code = 0;
      } else {
        switch (installer_type) {
          case MSI_INSTALLER:
            completion_info->status = COMPLETION_INSTALLER_ERROR_MSI;
            break;
          case UNKNOWN_INSTALLER:
          case CUSTOM_INSTALLER:
          case MAX_INSTALLER:
          default:
            completion_info->status = COMPLETION_INSTALLER_ERROR_OTHER;
            break;
        }
      }
      break;
    case INSTALLER_RESULT_MAX:
    default:
      ASSERT1(false);
      break;
  }

  // Handle the reboot required case.
  if ((COMPLETION_INSTALLER_ERROR_MSI == completion_info->status ||
       COMPLETION_INSTALLER_ERROR_SYSTEM == completion_info->status) &&
      (ERROR_SUCCESS_REBOOT_REQUIRED == completion_info->error_code)) {
    // This is a success, but the user should be notified.
    completion_info->status = COMPLETION_SUCCESS_REBOOT_REQUIRED;
    completion_info->error_code = 0;
  }

  // CompletionInfo status has been finalized. Handle launch command and make
  // sure all errors have error strings.
  switch (completion_info->status) {
    case COMPLETION_SUCCESS: {
        CString cmd_line;
        if (SUCCEEDED(RegKey::GetValue(app_client_state_key,
                                       kRegValueInstallerSuccessLaunchCmdLine,
                                       &cmd_line)) &&
            !cmd_line.IsEmpty()) {
          job_->set_launch_cmd_line(cmd_line);
        }
      }
      break;
    case COMPLETION_SUCCESS_REBOOT_REQUIRED:
      break;
    case COMPLETION_INSTALLER_ERROR_MSI:
    case COMPLETION_INSTALLER_ERROR_SYSTEM:
      ASSERT1(completion_info->text.IsEmpty());
      GetSystemErrorString(completion_info->error_code, &completion_info->text);
      break;
    case COMPLETION_INSTALLER_ERROR_OTHER:
      if (completion_info->text.IsEmpty()) {
        completion_info->text.FormatMessage(
              IDS_INSTALLER_FAILED_NO_MESSAGE,
              FormatErrorCode(completion_info->error_code));
      }
      break;
    case COMPLETION_ERROR:
    case COMPLETION_CANCELLED:
    default:
      ASSERT1(false);
  }

  OPT_LOG(L1, (_T("[%s]"), completion_info->ToString()));
  CleanupInstallerResultRegistry(app_guid);
}

void InstallManager::CleanupInstallerResultRegistry(const CString& app_guid) {
  CString app_client_state_key =
      goopdate_utils::GetAppClientStateKey(is_machine_, app_guid);
  CString update_key = ConfigManager::Instance()->registry_update(is_machine_);

  // Delete the old LastXXX values.  These may not exist, so don't care if they
  // fail.
  RegKey::DeleteValue(app_client_state_key,
                      kRegValueLastInstallerResult);
  RegKey::DeleteValue(app_client_state_key,
                      kRegValueLastInstallerResultUIString);
  RegKey::DeleteValue(app_client_state_key,
                      kRegValueLastInstallerError);
  RegKey::DeleteValue(app_client_state_key,
                      kRegValueLastInstallerSuccessLaunchCmdLine);

  // Also delete any values from Google\Update.
  // TODO(Omaha): This is a temporary fix for bug 1539293. Need a better
  // long-term solution.
  RegKey::DeleteValue(update_key,
                      kRegValueLastInstallerResult);
  RegKey::DeleteValue(update_key,
                      kRegValueLastInstallerResultUIString);
  RegKey::DeleteValue(update_key,
                      kRegValueLastInstallerError);
  RegKey::DeleteValue(update_key,
                      kRegValueLastInstallerSuccessLaunchCmdLine);

  // Rename current InstallerResultXXX values to LastXXX.
  RegKey::RenameValue(app_client_state_key,
                      kRegValueInstallerResult,
                      kRegValueLastInstallerResult);
  RegKey::RenameValue(app_client_state_key,
                      kRegValueInstallerError,
                      kRegValueLastInstallerError);
  RegKey::RenameValue(app_client_state_key,
                      kRegValueInstallerResultUIString,
                      kRegValueLastInstallerResultUIString);
  RegKey::RenameValue(app_client_state_key,
                      kRegValueInstallerSuccessLaunchCmdLine,
                      kRegValueLastInstallerSuccessLaunchCmdLine);

  // Copy over to the Google\Update key.
  // TODO(Omaha): This is a temporary fix for bug 1539293. Need a better
  // long-term solution.
  RegKey::CopyValue(app_client_state_key,
                    update_key,
                    kRegValueLastInstallerResult);
  RegKey::CopyValue(app_client_state_key,
                    update_key,
                    kRegValueLastInstallerError);
  RegKey::CopyValue(app_client_state_key,
                    update_key,
                    kRegValueLastInstallerResultUIString);
  RegKey::CopyValue(app_client_state_key,
                    update_key,
                    kRegValueLastInstallerSuccessLaunchCmdLine);
}

HRESULT InstallManager::CheckApplicationRegistration(
    const CString& previous_version) {
  CORE_LOG(L2,
           (_T("[InstallManager::CheckApplicationRegistration][%s]"),
            GuidToString(job_->app_data().app_guid())));
  ASSERT1(!::IsEqualGUID(kGoopdateGuid, job_->app_data().app_guid()));

  AppManager app_manager(is_machine_);
  if (!app_manager.IsProductRegistered(job_->app_data().app_guid())) {
    OPT_LOG(LE, (_T("[Installer did not register][%s]"),
                 GuidToString(job_->app_data().app_guid())));
    job_->set_extra_code1(-1);
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY;
  }

  ProductData product_data;
  // TODO(omaha): Should we check that each individual component was registered?
  HRESULT hr = app_manager.ReadProductDataFromStore(job_->app_data().app_guid(),
                                                    &product_data);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CheckApplicationRegistration]")
                  _T("[ReadProductDataFromStore failed][guid=%s][0x%08x]"),
                  GuidToString(job_->app_data().app_guid()), hr));
    job_->set_extra_code1(hr);
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY;
  }

  CORE_LOG(L2, (_T("[CheckApplicationRegistration]")
                _T("[guid=%s][version=%s][previous_version=%s]"),
                GuidToString(job_->app_data().app_guid()),
                product_data.app_data().version(),
                previous_version));

  if (product_data.app_data().version().IsEmpty()) {
    OPT_LOG(LE, (_T("[Installer did not write version][%s]"),
                 GuidToString(job_->app_data().app_guid())));
    job_->set_extra_code1(-2);
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY;
  }

  if (job_->is_update() &&
      previous_version == product_data.app_data().version()) {
    OPT_LOG(LE, (_T("[Installer did not change version][%s]"),
                 GuidToString(job_->app_data().app_guid())));
    return GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION;
  }

  // TODO(omaha): Log warning if version does not match version in config when
  // present.

  return S_OK;
}

// This method is used extensively for unit testing installation because it
// is the highest level method that returns an error.
// Assumes installer_lock_ has been initialized.
HRESULT InstallManager::DoInstallation() {
  OPT_LOG(L1, (_T("[InstallManager::DoInstallation][%s][%s][%s][%s]"),
               job_->app_data().display_name(),
               GuidToString(job_->app_data().app_guid()),
               job_->download_file_name(),
               job_->response_data().arguments()));

  CString executable_name;
  CString command_line;
  InstallerType installer_type = UNKNOWN_INSTALLER;

  CString arguments(job_->response_data().arguments());
  const TCHAR* const kChromeGuid = _T("{8A69D345-D564-463C-AFF1-A69D9E530F96}");
  const TCHAR* const kChromePerMachineArg = _T("--system-level");
  if (::IsEqualGUID(StringToGuid(kChromeGuid), job_->app_data().app_guid()) &&
      is_machine_) {
    arguments.AppendFormat(_T(" %s"), kChromePerMachineArg);
  }

  CString installer_data;
  HRESULT hr = job_->GetInstallerData(&installer_data);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetInstallerData failed][0x%x]"), hr));
    return hr;
  }

  hr = BuildCommandLineFromFilename(job_->download_file_name(),
                                    arguments,
                                    installer_data,
                                    &executable_name,
                                    &command_line,
                                    &installer_type);

  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[BuildCommandLineFromFilename failed][0x%08x]"), hr));
    ASSERT1(GOOPDATEINSTALL_E_FILENAME_INVALID == hr);
    return hr;
  }

  // We need this scope for the mutex.
  {
    // Acquire the global lock here. This will ensure that we are the only
    // installer running of the multiple goopdates.
    __mutexScope(installer_lock_);

    hr = ExecuteAndWaitForInstaller(executable_name,
                                    command_line,
                                    GuidToString(job_->app_data().app_guid()),
                                    installer_type);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[ExecuteAndWaitForInstaller failed][0x%08x][%s]"),
                    hr, GuidToString(job_->app_data().app_guid())));
    }
  }

  return hr;
}

HRESULT InstallManager::InstallDownloadedFile() {
  CString previous_version = job_->app_data().previous_version();

  HRESULT hr = DoInstallation();
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[DoInstallation failed][0x%08x]"), hr));
    return hr;
  }

  if (::IsEqualGUID(kGoopdateGuid, job_->app_data().app_guid())) {
    // Do not check application registration because the installer has not
    // completed.
    return S_OK;
  }

  // Ensure that the app installer wrote the minimum required registry values.
  hr = CheckApplicationRegistration(previous_version);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

InstallManager::InstallerResult InstallManager::GetInstallerResultType(
    const CString& app_client_state_key) {
  InstallerResult installer_result = INSTALLER_RESULT_DEFAULT;
  if (SUCCEEDED(RegKey::GetValue(
      app_client_state_key,
      kRegValueInstallerResult,
      reinterpret_cast<DWORD*>(&installer_result)))) {
    CORE_LOG(L2, (_T("[InstallerResult in registry][%u]"), installer_result));
  }
  if (INSTALLER_RESULT_MAX <= installer_result) {
    CORE_LOG(LW, (_T("[Unsupported InstallerResult value]")));
    installer_result = INSTALLER_RESULT_DEFAULT;
  }

  return installer_result;
}

void InstallManager::ReadInstallerErrorOverride(
    const CString& app_client_state_key,
    DWORD* installer_error) {
  ASSERT1(installer_error);
  if (FAILED(RegKey::GetValue(app_client_state_key,
                              kRegValueInstallerError,
                              installer_error))) {
    CORE_LOG(LW, (_T("[InstallerError not found in registry]")));
  }
}

void InstallManager::ReadInstallerResultUIStringOverride(
    const CString& app_client_state_key,
    CString* installer_result_uistring) {
  ASSERT1(installer_result_uistring);
  if (FAILED(RegKey::GetValue(app_client_state_key,
                              kRegValueInstallerResultUIString,
                              installer_result_uistring))) {
    CORE_LOG(LW, (_T("[InstallerResultUIString not found in registry]")));
  }
}

}  // namespace omaha
