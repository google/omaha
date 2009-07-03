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

#include "omaha/setup/setup_google_update.h"

#include <msi.h>
#include <atlpath.h>
#include <vector>
#include "base/basictypes.h"
#include "goopdate\google_update_idl.h"  // NOLINT
#include "omaha/common/app_util.h"
#include "omaha/common/atlregmapex.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/service_utils.h"
#include "omaha/common/utils.h"
#include "omaha/common/user_info.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource.h"
#include "omaha/setup/setup_metrics.h"
#include "omaha/setup/setup_service.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/ping_event.h"
#include "omaha/worker/ping_utils.h"

namespace omaha {

namespace {

#ifdef _DEBUG
HRESULT VerifyCOMLocalServerRegistration(bool is_machine) {
  // Validate the following:
  // * LocalServer32 under CLSID_OnDemandMachineAppsClass or
  //   CLSID_OnDemandUserAppsClass should be ...Google\Update\GoogleUpdate.exe.
  // * InProcServer32 under CLSID of IID_IGoogleUpdate should be
  //   ...Google\Update\{version}\goopdate.dll.
  // * ProxyStubClsid32 under IGoogleUpdate interface should be the CLSID of the
  //   proxy, which is IID_IGoogleUpdate.

  CString base_clsid_key(goopdate_utils::GetHKRoot());
  base_clsid_key += _T("\\Software\\Classes\\CLSID\\");
  CString ondemand_clsid_key(base_clsid_key);
  ondemand_clsid_key += GuidToString(is_machine ?
                                     __uuidof(OnDemandMachineAppsClass) :
                                     __uuidof(OnDemandUserAppsClass));
  CString local_server_key(ondemand_clsid_key + _T("\\LocalServer32"));
  CString installed_server;
  ASSERT1(SUCCEEDED(RegKey::GetValue(local_server_key,
                                     NULL,
                                     &installed_server)));
  ASSERT1(!installed_server.IsEmpty());

  CString expected_server(app_util::GetModulePath(NULL));
  EnclosePath(&expected_server);
  ASSERT1(!expected_server.IsEmpty());
  SETUP_LOG(L3, (_T("[installed_server=%s][expected_server=%s]"),
                 installed_server, expected_server));
  ASSERT1(installed_server == expected_server);

  const GUID proxy_clsid = PROXY_CLSID_IS;
  CString ondemand_proxy_clsid_key(base_clsid_key);
  ondemand_proxy_clsid_key += GuidToString(proxy_clsid);
  CString inproc_server_key(ondemand_proxy_clsid_key + _T("\\InProcServer32"));
  ASSERT1(SUCCEEDED(RegKey::GetValue(inproc_server_key,
                                     NULL,
                                     &installed_server)));
  ASSERT1(!installed_server.IsEmpty());
  expected_server = app_util::GetCurrentModulePath();
  ASSERT1(!expected_server.IsEmpty());
  SETUP_LOG(L3, (_T("[installed proxy=%s][expected proxy=%s]"),
                 installed_server, expected_server));
  ASSERT1(installed_server == expected_server);

  CString igoogleupdate_interface_key(goopdate_utils::GetHKRoot());
  igoogleupdate_interface_key += _T("\\Software\\Classes\\Interface\\");
  igoogleupdate_interface_key += GuidToString(__uuidof(IGoogleUpdate));
  igoogleupdate_interface_key += _T("\\ProxyStubClsid32");
  CString proxy_interface_value;
  ASSERT1(SUCCEEDED(RegKey::GetValue(igoogleupdate_interface_key,
                                     NULL,
                                     &proxy_interface_value)));
  ASSERT1(!proxy_interface_value.IsEmpty());
  ASSERT1(proxy_interface_value == GuidToString(proxy_clsid));

  return S_OK;
}
#endif

}  // namespace

SetupGoogleUpdate::SetupGoogleUpdate(bool is_machine,
                                     const CommandLineArgs* args)
    : is_machine_(is_machine),
#ifdef _DEBUG
      have_called_uninstall_previous_versions_(false),
#endif
      args_(args) {
  this_version_ = GetVersionString();
}

SetupGoogleUpdate::~SetupGoogleUpdate() {
  SETUP_LOG(L2, (_T("[SetupGoogleUpdate::~SetupGoogleUpdate]")));
}

// TODO(omaha): Add a VerifyInstall() method that can be called by /handoff
// instances to verify the installation and call FinishInstall() if it fails.

// Assumes the caller is ensuring this is the only running instance of setup.
// The original process holds the lock while it waits for this one to complete.
HRESULT SetupGoogleUpdate::FinishInstall() {
  SETUP_LOG(L2, (_T("[SetupGoogleUpdate::FinishInstall]")));

  HRESULT hr = InstallRegistryValues();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[InstallRegistryValues failed][0x%08x]"), hr));
    return hr;
  }

  hr = InstallLaunchMechanisms();
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[InstallLaunchMechanisms failed][0x%08x]"), hr));
    return hr;
  }

  hr = InstallMsiHelper();
  if (FAILED(hr)) {
    SETUP_LOG(L1, (_T("[InstallMsiHelper failed][0x%08x]"), hr));
    // TODO(omaha): Retry on ERROR_INSTALL_ALREADY_RUNNING like InstallManager
    // if we move helper MSI installation after app installation.
    ASSERT1(HRESULT_FROM_WIN32(ERROR_INSTALL_SERVICE_FAILURE) == hr ||
            HRESULT_FROM_WIN32(ERROR_INSTALL_ALREADY_RUNNING) == hr);
  }

  HRESULT hr_register_com_server(RegisterOrUnregisterCOMLocalServer(true));
  ASSERT1(SUCCEEDED(hr_register_com_server));
  if (FAILED(hr_register_com_server)) {
    OPT_LOG(L3, (_T("[RegisterOrUnregisterCOMLocalServer failed][0x%x]"),
                 hr_register_com_server));
    Ping ping;
    VERIFY1(SUCCEEDED(ping_utils::SendGoopdatePing(
                                      is_machine_,
                                      args_->extra,
                                      PingEvent::EVENT_SETUP_COM_SERVER_FAILURE,
                                      hr_register_com_server,
                                      0,
                                      NULL,
                                      &ping)));
  }

  ASSERT1(SUCCEEDED(VerifyCOMLocalServerRegistration(is_machine_)));

  // We would prefer to uninstall previous versions last, but the web plugin
  // requires that the old plugin is uninstalled before installing the new one.
  VERIFY1(SUCCEEDED(UninstallPreviousVersions()));
  VERIFY1(SUCCEEDED(InstallOptionalComponents()));

  // Writing this value indicates that this Omaha version was successfully
  // installed.
  CString reg_update = ConfigManager::Instance()->registry_update(is_machine_);
  hr = RegKey::SetValue(reg_update, kRegValueInstalledVersion, this_version_);
  if (FAILED(hr)) {
    return hr;
  }

  // Delete the "LastChecked" registry value after a successful install or
  // update so that Omaha checks for updates soon after the install. This
  // helps detecting a heart beat from the new version sooner as well as
  // avoiding deferring application updates for too long in the case where both
  // Omaha and application updates are available.
  RegKey::DeleteValue(reg_update, kRegValueLastChecked);

  return S_OK;
}


HRESULT SetupGoogleUpdate::InstallRegistryValues() {
  OPT_LOG(L3, (_T("[SetupGoogleUpdate::InstallRegistryValues]")));

  const ConfigManager* cm = ConfigManager::Instance();
  const TCHAR* keys[] = { cm->registry_google(is_machine_),
                          cm->registry_update(is_machine_),
                          cm->registry_client_state(is_machine_),
                          cm->registry_clients(is_machine_),
                          cm->registry_clients_goopdate(is_machine_),
                          cm->registry_client_state_goopdate(is_machine_),
                        };

  HRESULT hr = RegKey::CreateKeys(keys, arraysize(keys));
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to create reg keys][0x%08x]"), hr));
    return hr;
  }

  if (is_machine_) {
    hr = CreateClientStateMedium();
    if (FAILED(hr)) {
      SETUP_LOG(L3, (_T("[CreateClientStateMedium failed][0x%08x]"), hr));
      return hr;
    }
  }

  CString shell_path = goopdate_utils::BuildGoogleUpdateExePath(is_machine_);
  if (shell_path.IsEmpty() || !File::Exists(shell_path)) {
    SETUP_LOG(LE, (_T("[Failed to get valid shell path]")));
    return E_FAIL;
  }
  hr = RegKey::SetValue(cm->registry_update(is_machine_),
                        kRegValueInstalledPath,
                        shell_path);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[Failed to write shell path][0x%08x]"), hr));
    return hr;
  }

  // Set all the information for this instance correctly in the registry.
  const CString goopdate_state_key_name =
      cm->registry_client_state_goopdate(is_machine_);

  // IID and branding are optional, so ignore errors.
  VERIFY1(SUCCEEDED(SetInstallationId()));
  VERIFY1(SUCCEEDED(goopdate_utils::SetGoogleUpdateBranding(
      goopdate_state_key_name,
      args_->extra.brand_code,
      args_->extra.client_id)));

  // Set pv in ClientState for consistency. Optional, so ignore errors.
  ASSERT1(!this_version_.IsEmpty());
  VERIFY1(SUCCEEDED(RegKey::SetValue(goopdate_state_key_name,
                                     kRegValueProductVersion,
                                     this_version_)));

  return S_OK;
}

// Creates the ClientStateMedium key and adds ACLs that allows authenticated
// users to read and write values in its subkeys.
// Since this key is not as secure as other keys, the supported values must be
// limited and the use of them must be carefully designed.
HRESULT SetupGoogleUpdate::CreateClientStateMedium() {
  ASSERT1(is_machine_);

  // Authenticated non-admins may read, write, and create values.
  const ACCESS_MASK kNonAdminAccessMask = KEY_READ | KEY_SET_VALUE;
  // The override privileges apply to all subkeys and values but not to the
  // ClientStateMedium key itself.
  const uint8 kAceFlags =
      CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE;

  const CString key_full_name =
      ConfigManager::Instance()->machine_registry_client_state_medium();
  HRESULT hr = RegKey::CreateKey(key_full_name);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[Create ClientStateMedium failed][0x%08x]"), hr));
    return hr;
  }

  // GetNamedSecurityInfo requires the key name start with "MACHINE".
  // TODO(omaha): Replace AddAllowedAce or add an override that takes a handle
  // instead of a name to eliminate this issue.
  CString compatible_key_name = key_full_name;
  VERIFY1(1 == compatible_key_name.Replace(MACHINE_KEY_NAME, _T("MACHINE")));

  hr = AddAllowedAce(compatible_key_name,
                     SE_REGISTRY_KEY,
                     Sids::Interactive(),
                     kNonAdminAccessMask,
                     kAceFlags);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[AddAllowedAce failed][%s][0x%08x]"),
                   key_full_name, hr));
    return hr;
  }

  return S_OK;
}

// Overwrites any existing IID for Omaha if a new one is specified.
HRESULT SetupGoogleUpdate::SetInstallationId() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::SetInstallationId]")));

  if (GUID_NULL != args_->extra.installation_id) {
    CString goopdate_state_key_name =
        ConfigManager::Instance()->registry_client_state_goopdate(is_machine_);
    return RegKey::SetValue(goopdate_state_key_name,
                            kRegValueInstallationId,
                            GuidToString(args_->extra.installation_id));
  }

  return S_OK;
}

HRESULT SetupGoogleUpdate::InstallLaunchMechanisms() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::InstallLaunchMechanisms]")));
  if (is_machine_) {
    HRESULT hr = InstallMachineLaunchMechanisms();
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[InstallMachineLaunchMechanisms fail][0x%08x]"), hr));
      return hr;
    }
  } else {
    HRESULT hr = InstallUserLaunchMechanisms();
    if (FAILED(hr)) {
      SETUP_LOG(LE, (_T("[InstallUserLaunchMechanisms failed][0x%08x]"), hr));
      return hr;
    }
  }

  return S_OK;
}

void SetupGoogleUpdate::UninstallLaunchMechanisms() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::UninstallLaunchMechanisms]")));
  if (is_machine_) {
    VERIFY1(SUCCEEDED(SetupService::UninstallService()));
  } else {
    // We only need to do this in case of the user goopdate, as
    // there is no machine Run at startup installation.
    VERIFY1(SUCCEEDED(ConfigureUserRunAtStartup(false)));  // delete entry
  }

  VERIFY1(SUCCEEDED(goopdate_utils::UninstallGoopdateTasks(is_machine_)));
}

HRESULT SetupGoogleUpdate::InstallScheduledTask() {
  CString exe_path = goopdate_utils::BuildGoogleUpdateExePath(is_machine_);

  HighresTimer metrics_timer;
  const ULONGLONG install_task_start_ms = metrics_timer.GetElapsedMs();

  HRESULT hr = goopdate_utils::InstallGoopdateTasks(exe_path, is_machine_);

  if (SUCCEEDED(hr)) {
    const ULONGLONG install_task_end_ms = metrics_timer.GetElapsedMs();
    ASSERT1(install_task_end_ms >= install_task_start_ms);
    metric_setup_install_task_ms.AddSample(
        install_task_end_ms - install_task_start_ms);
    ++metric_setup_install_task_succeeded;
  } else {
    OPT_LOG(LEVEL_ERROR, (_T("[Install task failed][0x%08x]"), hr));
    metric_setup_install_task_error = hr;
  }

  return hr;
}

// Assumes the any existing service instance has been stopped
// TODO(omaha): Provide service_hr and task_hr failures in a ping.
// They are no longer being provided in the URL.
HRESULT SetupGoogleUpdate::InstallMachineLaunchMechanisms() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::InstallMachineLaunchMechanisms]")));
  ++metric_setup_install_service_task_total;

  // Install the service and scheduled task. Failing to install both will
  // fail setup.
  OPT_LOG(L1, (_T("[Installing service]")));
  HighresTimer metrics_timer;

  HRESULT service_hr = SetupService::InstallService(
      goopdate_utils::BuildGoogleUpdateExePath(is_machine_));

  if (SUCCEEDED(service_hr)) {
    metric_setup_install_service_ms.AddSample(
        metrics_timer.GetElapsedMs());
    ++metric_setup_install_service_succeeded;
  } else {
    metric_setup_install_service_failed_ms.AddSample(
        metrics_timer.GetElapsedMs());
    OPT_LOG(LEVEL_ERROR, (_T("[Install service failed][0x%08x]"), service_hr));
    metric_setup_install_service_error = service_hr;
  }

  HRESULT task_hr = InstallScheduledTask();

  if (FAILED(service_hr) && FAILED(task_hr)) {
    ++metric_setup_install_service_and_task_failed;
    return service_hr;
  }
  if (args_->is_oem_set) {
    // OEM installs are on clean systems in a controlled environment. We expect
    // both mechanisms to install.
    if (FAILED(service_hr)) {
      return service_hr;
    }
    if (FAILED(task_hr)) {
      return task_hr;
    }
  }

  if (SUCCEEDED(service_hr) && SUCCEEDED(task_hr)) {
    ++metric_setup_install_service_and_task_succeeded;
  }

  return S_OK;
}

HRESULT SetupGoogleUpdate::InstallUserLaunchMechanisms() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::InstallUserLaunchMechanisms]")));

  HRESULT run_hr = ConfigureUserRunAtStartup(true);  // install
  ASSERT(SUCCEEDED(run_hr), (_T("ConfigureRunAtStartup 0x%x"), run_hr));

  HRESULT task_hr = InstallScheduledTask();
  ASSERT(SUCCEEDED(task_hr), (_T("InstallScheduledTask 0x%x"), task_hr));

  if (FAILED(run_hr) && FAILED(task_hr)) {
    // We need atleast one launch mechanism.
    return run_hr;
  }

  return S_OK;
}

// Sets a value in the Run key in the user registry to start the core.
HRESULT SetupGoogleUpdate::ConfigureUserRunAtStartup(bool install) {
  SETUP_LOG(L3, (_T("SetupGoogleUpdate::ConfigureUserRunAtStartup")));
  // Always send false argument as this method is only called for user
  // goopdate installs.
  CString core_cmd = BuildCoreProcessCommandLine();
  return ConfigureRunAtStartup(USER_KEY_NAME, kRunValueName, core_cmd, install);
}

HRESULT SetupGoogleUpdate::RegisterOrUnregisterCOMLocalServer(bool reg) {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::RegisterOrUnregisterCOMLocalServer]")
                 _T("[%d]"), reg));
  CString google_update_path = goopdate_utils::BuildGoogleUpdateExePath(
                                   is_machine_);
  CString register_cmd;
  register_cmd.Format(_T("/%s"), reg ? _T("RegServer") : _T("UnregServer"));
  HRESULT hr = RegisterOrUnregisterExe(google_update_path, register_cmd);
  if (FAILED(hr)) {
    SETUP_LOG(LE, (_T("[RegisterOrUnregisterExe failed][0x%08x]"), hr));
    return hr;
  }
  return S_OK;
}

// Assumes that the MSI is in the current directory.
// To debug MSI failures, use the following statement:
// ::MsiEnableLog(INSTALLLOGMODE_VERBOSE, _T("C:\\msi.log"), NULL);
HRESULT SetupGoogleUpdate::InstallMsiHelper() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::InstallMsiHelper]")));
  if (!is_machine_) {
    return S_OK;
  }

  ++metric_setup_helper_msi_install_total;
  HighresTimer metrics_timer;

  CString msi_path(app_util::GetCurrentModuleDirectory());
  if (!::PathAppend(CStrBuf(msi_path, MAX_PATH), kHelperInstallerName)) {
    ASSERT1(false);
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }

  // Setting INSTALLUILEVEL_NONE causes installation to be silent and not
  // create a restore point.
  ::MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);

  // Try a normal install.
  UINT res = ::MsiInstallProduct(msi_path, _T(""));
  if (ERROR_PRODUCT_VERSION == res) {
    // The product may already be installed. Force a reinstall of everything.
    SETUP_LOG(L3, (_T("[ERROR_PRODUCT_VERSION returned - forcing reinstall]")));
    res = ::MsiInstallProduct(msi_path,
                              _T("REINSTALL=ALL REINSTALLMODE=vamus"));
  }

  HRESULT hr = HRESULT_FROM_WIN32(res);
  if (FAILED(hr)) {
    SETUP_LOG(L1, (_T("[MsiInstallProduct failed][0x%08x][%u]"), hr, res));
    return hr;
  }

  metric_setup_helper_msi_install_ms.AddSample(metrics_timer.GetElapsedMs());
  ++metric_setup_helper_msi_install_succeeded;
  return S_OK;
}

// The MSI is uninstalled.
// TODO(omaha): Make sure this works after deleting the MSI.
HRESULT SetupGoogleUpdate::UninstallMsiHelper() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::UninstallMsiHelper]")));
  if (!is_machine_) {
    return S_OK;
  }

  // Setting INSTALLUILEVEL_NONE causes installation to be silent and not
  // create a restore point.
  ::MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
  UINT res = ::MsiConfigureProduct(kHelperInstallerProductGuid,
                                   INSTALLLEVEL_DEFAULT,
                                   INSTALLSTATE_ABSENT);

  // Ignore the product not currently installed result.
  if ((ERROR_SUCCESS != res) && (ERROR_UNKNOWN_PRODUCT != res)) {
    HRESULT hr = HRESULT_FROM_WIN32(res);
    if (FAILED(hr)) {
      SETUP_LOG(L1, (_T("[MsiInstallProduct failed][0x%08x][%u]"), hr, res));
      return hr;
    }
  }

  return S_OK;
}

HRESULT SetupGoogleUpdate::InstallOptionalComponents() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::InstallOptionalComponents]")));
  ASSERT1(have_called_uninstall_previous_versions_);
  // Failure of registration of optional components is acceptable in release
  // builds.
  HRESULT hr = S_OK;

  CString goopdate_oneclick_path =
      BuildSupportFileInstallPath(ACTIVEX_FILENAME);
  hr = RegisterDll(goopdate_oneclick_path);
  if (FAILED(hr)) {
    SETUP_LOG(L1, (_T("[Register OneClick DLL failed][0x%08x]"), hr));
  }

#if 0
  // Only install the BHO for machine installs. There is no corresponding HKCU
  // registration for BHOs.
  if (is_machine_) {
    CString goopdate_bho_proxy_path = BuildSupportFileInstallPath(BHO_FILENAME);
    hr = RegisterDll(goopdate_bho_proxy_path);
    if (FAILED(hr)) {
      SETUP_LOG(L1, (_T("[Register bho_proxy DLL failed][0x%08x]"), hr));
    }
  }
#endif

  return hr;
}

HRESULT SetupGoogleUpdate::UninstallOptionalComponents() {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::UninstallOptionalComponents]")));
  // Unregistration. Failure is acceptable in release builds.
  HRESULT hr = S_OK;

  CString goopdate_oneclick_path =
      BuildSupportFileInstallPath(ACTIVEX_FILENAME);
  hr = UnregisterDll(goopdate_oneclick_path);
  if (FAILED(hr)) {
    SETUP_LOG(L1, (_T("[Unregister OneClick DLL failed][0x%08x]"), hr));
  }

#if 0
  if (is_machine_) {
    CString goopdate_bho_proxy_path = BuildSupportFileInstallPath(BHO_FILENAME);
    hr = UnregisterDll(goopdate_bho_proxy_path);
    if (FAILED(hr)) {
      SETUP_LOG(L1, (_T("[Unregister bho_proxy DLL failed][0x%08x]"), hr));
    }
  }
#endif

  return hr;
}

CString SetupGoogleUpdate::BuildSupportFileInstallPath(
    const CString& filename) const {
  SETUP_LOG(L3, (_T("[SetupGoogleUpdate::BuildSupportFileInstallPath][%s]"),
                 filename));
  CPath install_file_path = goopdate_utils::BuildInstallDirectory(
                                is_machine_,
                                this_version_);
  VERIFY1(install_file_path.Append(filename));

  return install_file_path;
}

CString SetupGoogleUpdate::BuildCoreProcessCommandLine() const {
  CommandLineBuilder builder(COMMANDLINE_MODE_CORE);
  CString google_update_path = goopdate_utils::BuildGoogleUpdateExePath(
                                   is_machine_);
  return builder.GetCommandLine(google_update_path);
}

HRESULT SetupGoogleUpdate::UninstallPreviousVersions() {
#ifdef _DEBUG
  have_called_uninstall_previous_versions_ = true;
#endif

  VERIFY1(SUCCEEDED(goopdate_utils::UninstallLegacyGoopdateTasks(is_machine_)));

  CString install_path(
      is_machine_ ? ConfigManager::Instance()->GetMachineGoopdateInstallDir() :
                    ConfigManager::Instance()->GetUserGoopdateInstallDir());
  SETUP_LOG(L1, (_T("[SetupGoogleUpdate::UninstallPreviousVersions][%s][%s]"),
                 install_path, this_version_));
  // An empty install_path can be disastrous as it will start deleting from the
  // current directory.
  ASSERT1(!install_path.IsEmpty());
  if (install_path.IsEmpty()) {
    return E_UNEXPECTED;
  }

  // In the Google\\Update directory, run over all files and directories.
  WIN32_FIND_DATA file_data = {0};
  CPath find_files(install_path);
  VERIFY1(find_files.Append(_T("*.*")));

  scoped_hfind find_handle(::FindFirstFile(find_files, &file_data));
  ASSERT1(find_handle);
  if (!find_handle) {
    // We should have found ".", "..", and our versioned directory.
    DWORD err = ::GetLastError();
    SETUP_LOG(LE, (L"[Subdirs not found under dir][%s][%d]", find_files, err));
    return HRESULT_FROM_WIN32(err);
  }

  BOOL found_next = TRUE;
  for (; found_next; found_next = ::FindNextFile(get(find_handle),
                                                 &file_data)) {
    CPath file_or_directory(install_path);
    VERIFY1(file_or_directory.Append(file_data.cFileName));
    if (!(file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      // Do not delete the shell as it is used by all versions.
      if (_tcsicmp(file_data.cFileName, kGoopdateFileName)) {
        DeleteBeforeOrAfterReboot(file_or_directory);
      }
    } else if (_tcscmp(file_data.cFileName, _T("..")) &&
               _tcscmp(file_data.cFileName, _T(".")) &&
               _tcsicmp(file_data.cFileName, this_version_) &&
               (!args_->is_offline_set ||
                _tcscmp(file_data.cFileName, OFFLINE_DIR_NAME))) {
      // Unregister the previous version OneClick if it exists. Ignore
      // failures. The file is named npGoogleOneClick*.dll.
      CPath old_oneclick(file_or_directory);
      VERIFY1(old_oneclick.Append(ACTIVEX_NAME _T("*.dll")));
      WIN32_FIND_DATA old_oneclick_file_data = {0};
      scoped_hfind found_oneclick(::FindFirstFile(old_oneclick,
                                                  &old_oneclick_file_data));
      if (found_oneclick) {
        CPath old_oneclick_file(file_or_directory);
        VERIFY1(old_oneclick_file.Append(old_oneclick_file_data.cFileName));
        VERIFY1(SUCCEEDED(UnregisterDll(old_oneclick_file)));
      }

      if (is_machine_) {
        // BHO is only installed for the machine case.
        // Unregister the previous version BHO if it exists. Ignore failures.
        CPath old_bho_dll(file_or_directory);
        VERIFY1(old_bho_dll.Append(BHO_FILENAME));
        if (File::Exists(old_bho_dll)) {
          if (FAILED(UnregisterDll(old_bho_dll))) {
            SETUP_LOG(LW, (L"[UnregisterDll() failed][%s]", old_bho_dll));
          }
        }
      }
      // Delete entire sub-directory.
      DeleteBeforeOrAfterReboot(file_or_directory);
    }
  }

  // Need to clean up old lang key for Goopdate.  Don't care about failure.
  CString goopdate_client_key =
      ConfigManager::Instance()->registry_clients_goopdate(is_machine_);
  RegKey::DeleteValue(goopdate_client_key, kRegValueLanguage);

  if (!found_next) {
    DWORD err = ::GetLastError();
    if (ERROR_NO_MORE_FILES != err) {
      SETUP_LOG(LE, (L"[::FindNextFile() failed][%d]", err));
      return HRESULT_FROM_WIN32(err);
    }
  }

  return S_OK;
}

void SetupGoogleUpdate::Uninstall() {
  OPT_LOG(L1, (_T("[SetupGoogleUpdate::Uninstall]")));

  HRESULT hr = UninstallOptionalComponents();
  if (FAILED(hr)) {
    SETUP_LOG(LW, (_T("[UninstallOptionalComponents failed][0x%08x]"), hr));
    ASSERT1(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND) == hr);
  }

  // TODO(omaha): Some of the checks assume that The uninstall is being
  // executed from the installed path. This is not the case when /install is
  // uninstalling and will also not be the case when Setup is merged into one
  // process.
  if (goopdate_utils::IsRunningFromOfficialGoopdateDir(is_machine_)) {
    ASSERT1(SUCCEEDED(VerifyCOMLocalServerRegistration(is_machine_)));
  }
  hr = RegisterOrUnregisterCOMLocalServer(false);
  if (FAILED(hr)) {
    SETUP_LOG(LW,
              (_T("[RegisterOrUnregisterCOMLocalServer failed][0x%08x]"), hr));
    ASSERT1(GOOGLEUPDATE_E_DLL_NOT_FOUND == hr ||
            HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
  }

  hr = UninstallMsiHelper();
  if (FAILED(hr)) {
    SETUP_LOG(L1, (_T("[UninstallMsiHelper failed][0x%08x]"), hr));
    ASSERT1(HRESULT_FROM_WIN32(ERROR_INSTALL_SERVICE_FAILURE) == hr);
  }

  UninstallLaunchMechanisms();

  // Remove everything under top level Google Update registry key.
  hr = DeleteRegistryKeys();
  ASSERT1(SUCCEEDED(hr) || HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr);
}

// Also deletes the main Google Update key if there is nothing in it.
HRESULT SetupGoogleUpdate::DeleteRegistryKeys() {
  OPT_LOG(L3, (_T("[SetupGoogleUpdate::DeleteRegistryKeys]")));

  CString root_key = ConfigManager::Instance()->registry_update(is_machine_);
  ASSERT1(!root_key.IsEmpty());

  RegKey root;
  HRESULT hr = root.Open(root_key);
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<CString> sub_keys;
  size_t num_keys = root.GetSubkeyCount();
  for (size_t i = 0; i < num_keys; ++i) {
    CString sub_key_name;
    hr = root.GetSubkeyNameAt(i, &sub_key_name);
    ASSERT1(hr == S_OK);
    if (SUCCEEDED(hr)) {
      sub_keys.push_back(sub_key_name);
    }
  }

  ASSERT1(num_keys == sub_keys.size());
  // Delete all the sub keys of the root key.
  for (size_t i = 0; i < num_keys; ++i) {
    VERIFY1(SUCCEEDED(root.RecurseDeleteSubKey(sub_keys[i])));
  }

  // Now delete all the values of the root key.
  // The mi and ui values are not deleted.
  // The Last* values are not deleted.
  // TODO(Omaha): The above a temporary fix for bug 1539293. Need a better
  // long-term solution.
  size_t num_values = root.GetValueCount();
  std::vector<CString> value_names;
  for (size_t i = 0; i < num_values; ++i) {
    CString value_name;
    DWORD type = 0;
    hr = root.GetValueNameAt(i, &value_name, &type);
    ASSERT1(hr == S_OK);
    // TODO(omaha): Remove kRegValueLast* once we have an install API.
    if (SUCCEEDED(hr)) {
      if (value_name != kRegValueMachineId &&
          value_name != kRegValueUserId &&
          value_name != kRegValueLastInstallerResult &&
          value_name != kRegValueLastInstallerError &&
          value_name != kRegValueLastInstallerResultUIString &&
          value_name != kRegValueLastInstallerSuccessLaunchCmdLine) {
        value_names.push_back(value_name);
      }
    }
  }

  for (size_t i = 0; i < value_names.size(); ++i) {
    VERIFY1(SUCCEEDED(root.DeleteValue(value_names[i])));
  }

  if (0 == root.GetValueCount() && 0 == root.GetSubkeyCount()) {
    VERIFY1(SUCCEEDED(RegKey::DeleteKey(root_key, false)));
  }

  return S_OK;
}

}  // namespace omaha
