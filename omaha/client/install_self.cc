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

#include "omaha/client/install_self.h"
#include "omaha/client/install_self_internal.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/base/xml_utils.h"
#include "omaha/client/client_utils.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/common/ping.h"
#include "omaha/common/ping_event.h"
#include "omaha/setup/setup.h"
#include "omaha/setup/setup_metrics.h"

namespace omaha {

namespace install_self {

namespace {

// Returns whether elevation is required.
bool IsElevationRequired(bool is_machine) {
  return is_machine && !vista_util::IsUserAdmin();
}

}  // namespace

namespace internal {

HRESULT DoSelfUpdate(bool is_machine, int* extra_code1) {
  ASSERT1(extra_code1);

  *extra_code1 = 0;

  HRESULT hr = DoInstallSelf(is_machine,
                             true,
                             false,
                             RUNTIME_MODE_NOT_SET,
                             extra_code1);
  if (FAILED(hr)) {
    PersistUpdateErrorInfo(is_machine, hr, *extra_code1, GetVersionString());
    return hr;
  }

  return S_OK;
}

// Does not need to update the UI during Omaha install. This should be quick
// with a simple UI. UI will transition when product install begins.
HRESULT DoInstallSelf(bool is_machine,
                      bool is_self_update,
                      bool is_eula_required,
                      RuntimeMode runtime_mode,
                      int* extra_code1) {
  ASSERT1(extra_code1);
  ASSERT1(!is_self_update || !is_eula_required);
  ASSERT1(!IsElevationRequired(is_machine));

  *extra_code1 = 0;

  // TODO(omaha3): This needs to be in some type of lock(s) when
  // !is_eula_required. See comments for SetEulaNotAccepted.
  // TODO(omaha3): Integrate CL 11232530 - fix for bug 1866730 - from mainline.
  // The Omaha 2 implementation of EULA [not] accepted was completely changed in
  // this CL, which has not been integrated yet.
  // TODO(omaha3): Code running in install-related processes before this point
  // need to check for /eularequired before sending pings OR we need to move
  // this before any pings can be sent. Even the latter is insufficient for
  // the non-elevated machine install instance on Vista.
  HRESULT hr = SetEulaRequiredState(is_machine, is_eula_required);
  if (FAILED(hr)) {
    return hr;
  }

  // Checking system requirements here keeps requirements checks for other
  // modules out of Setup.
  // It is possible that an older metainstaller would fail the install for
  // system requirements that are not required for the installed version when
  // doing a handoff install.
  hr = CheckSystemRequirements();
  if (FAILED(hr)) {
    return hr;
  }

  Setup setup(is_machine);
  setup.set_is_self_update(is_self_update);
  hr = setup.Install(runtime_mode);
  *extra_code1 = setup.extra_code1();

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Setup::Install failed][0x%08x]"), hr));
    return hr;
  }

  // All Omaha installs are "offline" because there is no update check.
  const CString omaha_client_state_key_path =
      ConfigManager::Instance()->registry_client_state_goopdate(is_machine);
  app_registry_utils::PersistSuccessfulInstall(omaha_client_state_key_path,
                                               is_self_update,
                                               !is_self_update);  // is_offline

  CORE_LOG(L1, (_T("[Setup successfully completed]")));

  return S_OK;
}

HRESULT CheckSystemRequirements() {
  // Validate that key OS components are installed.
  if (!HasXmlParser()) {
    return GOOPDATE_E_RUNNING_INFERIOR_MSXML;
  }

  return S_OK;
}

bool HasXmlParser() {
  CComPtr<IXMLDOMDocument> my_xmldoc;
  HRESULT hr = CoCreateSafeDOMDocument(&my_xmldoc);
  const bool ret = SUCCEEDED(hr);
  CORE_LOG(L3, (_T("[HasXmlParser returned %d][0x%08x]"), ret, hr));
  return ret;
}

// Failing to set the state fails installation because this would prevent
// updates or allow updates that should not be allowed.
HRESULT SetEulaRequiredState(bool is_machine, bool is_eula_required) {
  ASSERT1(!IsElevationRequired(is_machine));

  const bool eula_accepted = !is_eula_required;
  HRESULT hr = eula_accepted ? SetEulaAccepted(is_machine) :
                               SetEulaNotAccepted(is_machine);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[set EULA accepted state failed][accepted=%d][0x%08x]"),
                   eula_accepted, hr));
    return hr;
  }

  return S_OK;
}

// Does not write the registry if Google Update is already installed as
// determined by the presence of 2 or more registered apps. In those cases, we
// assume the existing EULA state is correct and do not want to disable updates
// for an existing installation.
// Assumes it is called with appropriate synchronization protection such that it
// can reliably check the number of registered clients.
// TODO(omaha3): How do we assure the above assumption?
HRESULT SetEulaNotAccepted(bool is_machine) {
  CORE_LOG(L4, (_T("[SetEulaNotAccepted][%d]"), is_machine));

  size_t num_clients(0);
  if (SUCCEEDED(app_registry_utils::GetNumClients(is_machine, &num_clients)) &&
      num_clients >= 2) {
    CORE_LOG(L4, (_T(" [Apps registered. Not setting eulaaccepted=0.]")));
    return S_OK;
  }

  const ConfigManager* cm = ConfigManager::Instance();
  return RegKey::SetValue(cm->registry_update(is_machine),
                          kRegValueOmahaEulaAccepted,
                          static_cast<DWORD>(0));
}

HRESULT SetInstallationId(const CString& omaha_client_state_key_path,
                          const GUID& iid) {
  if (GUID_NULL != iid) {
    return RegKey::SetValue(omaha_client_state_key_path,
                            kRegValueInstallationId,
                            GuidToString(iid));
  }

  return S_OK;
}

void PersistUpdateErrorInfo(bool is_machine,
                            HRESULT error,
                            int extra_code1,
                            const CString& version) {
  const TCHAR* update_key_name =
      ConfigManager::Instance()->registry_update(is_machine);
  VERIFY_SUCCEEDED(RegKey::SetValue(update_key_name,
                                     kRegValueSelfUpdateErrorCode,
                                     static_cast<DWORD>(error)));
  VERIFY_SUCCEEDED(RegKey::SetValue(update_key_name,
                                     kRegValueSelfUpdateExtraCode1,
                                     static_cast<DWORD>(extra_code1)));
  VERIFY_SUCCEEDED(RegKey::SetValue(update_key_name,
                                     kRegValueSelfUpdateVersion,
                                     version));
}

}  // namespace internal

// Returns false if the values cannot be deleted to avoid skewing the log data
// with a single user pinging repeatedly with the same data.
bool ReadAndClearUpdateErrorInfo(bool is_machine,
                                 DWORD* error_code,
                                 DWORD* extra_code1,
                                 CString* version) {
  ASSERT1(error_code);
  ASSERT1(extra_code1);
  ASSERT1(version);

  const TCHAR* update_key_name =
      ConfigManager::Instance()->registry_update(is_machine);
  RegKey update_key;
  HRESULT hr = update_key.Open(update_key_name);
  if (FAILED(hr)) {
    ASSERT1(false);
    return false;
  }

  if (!update_key.HasValue(kRegValueSelfUpdateErrorCode)) {
    ASSERT1(!update_key.HasValue(kRegValueSelfUpdateExtraCode1));
    return false;
  }

  VERIFY_SUCCEEDED(update_key.GetValue(kRegValueSelfUpdateErrorCode,
                                        error_code));
  ASSERT1(FAILED(*error_code));

  VERIFY_SUCCEEDED(update_key.GetValue(kRegValueSelfUpdateExtraCode1,
                                        extra_code1));

  VERIFY_SUCCEEDED(update_key.GetValue(kRegValueSelfUpdateVersion, version));

  if (FAILED(update_key.DeleteValue(kRegValueSelfUpdateErrorCode)) ||
      FAILED(update_key.DeleteValue(kRegValueSelfUpdateExtraCode1)) ||
      FAILED(update_key.DeleteValue(kRegValueSelfUpdateVersion))) {
    ASSERT1(false);
    return false;
  }

  return true;
}

HRESULT SetEulaAccepted(bool is_machine) {
  CORE_LOG(L4, (_T("[SetEulaAccepted][%d]"), is_machine));
  const TCHAR* update_key_name =
      ConfigManager::Instance()->registry_update(is_machine);
  return RegKey::HasKey(update_key_name) ?
      RegKey::DeleteValue(update_key_name, kRegValueOmahaEulaAccepted) :
      S_OK;
}

HRESULT InstallSelf(bool is_machine,
                    bool is_eula_required,
                    bool is_oem_install,
                    bool is_enterprise_install,
                    const CString& current_version,
                    const CString& install_source,
                    const CommandLineExtraArgs& extra_args,
                    const CString& session_id,
                    int* extra_code1) {
  CORE_LOG(L2, (_T("[InstallSelf]")));
  time64 install_start_time = GetCurrentMsTime();

  HRESULT hr = internal::DoInstallSelf(is_machine,
                                       false,
                                       is_eula_required,
                                       extra_args.runtime_mode,
                                       extra_code1);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[DoInstallSelf failed][0x%08x]"), hr));
    return hr;
  }

  // Set Omaha's optional IID, experiment labels, and branding.
  // IID and experiment labels are always written on install; branding will
  // only be written if no branding exists, which should only be true on the
  // first install.
  const CString omaha_client_state_key_path =
      ConfigManager::Instance()->registry_client_state_goopdate(is_machine);

  // TODO(omaha): move SetInstallationId to app_registry_utils
  VERIFY_SUCCEEDED(internal::SetInstallationId(omaha_client_state_key_path,
                                                extra_args.installation_id));
  VERIFY_SUCCEEDED(ExperimentLabels::WriteRegistry(
      is_machine, kGoogleUpdateAppId, extra_args.experiment_labels));
  VERIFY_SUCCEEDED(app_registry_utils::SetGoogleUpdateBranding(
      omaha_client_state_key_path,
      extra_args.brand_code,
      extra_args.client_id));

  if (is_eula_required || is_oem_install || is_enterprise_install) {
    return S_OK;
  }

  time64 install_end_time = GetCurrentMsTime();
  ASSERT1(install_end_time >= install_start_time);
  int install_time = static_cast<int>(install_end_time - install_start_time);

  // Send a successful EVENT_INSTALL_COMPLETE ping and do not wait for the
  // completion of the ping. This reduces the overall latency of Omaha
  // installs. The 'curent_version' parameter represent the version of
  // Omaha before the setup has run.
  Ping install_ping(is_machine, session_id, install_source);
  PingEventPtr setup_install_complete_ping_event(
      new PingEvent(PingEvent::EVENT_INSTALL_COMPLETE,
                    PingEvent::EVENT_RESULT_SUCCESS,
                    hr,
                    *extra_code1,
                    -1,  // No source URL to report.
                    0,  // No update check time to report.
                    0,
                    0,
                    0,  // App size 0 so no download time report.
                    install_time));
  const CString next_version(GetVersionString());
  install_ping.LoadAppDataFromExtraArgs(extra_args);
  install_ping.BuildOmahaPing(current_version,
                              next_version,
                              setup_install_complete_ping_event);
  HRESULT send_result = SendReliablePing(&install_ping, true);
  if (FAILED(send_result)) {
    CORE_LOG(LW, (_T("[SendReliablePing failed][%#x]"), send_result));
  }

  return S_OK;
}

HRESULT UpdateSelf(bool is_machine, const CString& session_id) {
  CORE_LOG(L2, (_T("[UpdateSelf]")));

  ++metric_setup_update_self_total;

  // 'current_version' corresponds to the value of 'pv' read from the registry.
  CString current_version;
  app_registry_utils::GetAppVersion(is_machine,
                                    kGoogleUpdateAppId,
                                    &current_version);

  int extra_code1 = 0;
  const HRESULT hr = internal::DoSelfUpdate(is_machine, &extra_code1);
  if (SUCCEEDED(hr)) {
    ++metric_setup_update_self_succeeded;
  } else {
    CORE_LOG(LE, (_T("[DoSelfUpdate failed][0x%08x]"), hr));
  }

  metric_omaha_last_error_code = hr;
  metric_omaha_last_extra_code = extra_code1;

  // If a self-update failed because an uninstall of that Omaha is in progress,
  // don't bother with an update failure ping; the uninstall ping will suffice.
  if (hr == GOOPDATE_E_FAILED_TO_GET_LOCK_UNINSTALL_PROCESS_RUNNING) {
    return hr;
  }

  if (!ConfigManager::Instance()->CanUseNetwork(is_machine)) {
    return hr;
  }

  // Send an update complete ping and wait for send to complete.
  PingEvent::Results result = SUCCEEDED(hr) ?
                              PingEvent::EVENT_RESULT_SUCCESS :
                              PingEvent::EVENT_RESULT_ERROR;

  const CString next_version(GetVersionString());

  PingEventPtr update_complete_ping_event(
      new PingEvent(PingEvent::EVENT_UPDATE_COMPLETE, result, hr, extra_code1));

  Ping ping(is_machine, session_id, kCmdLineInstallSource_SelfUpdate);
  ping.LoadOmahaDataFromRegistry();
  ping.BuildOmahaPing(current_version,
                      next_version,
                      update_complete_ping_event);
  SendReliablePing(&ping, false);

  return hr;
}

HRESULT Repair(bool is_machine) {
  CORE_LOG(L2, (_T("[Repair]")));
  int extra_code1 = 0;
  return internal::DoSelfUpdate(is_machine, &extra_code1);
}

void CheckInstallStateConsistency(bool is_machine) {
  Setup::CheckInstallStateConsistency(is_machine);
}

HRESULT UninstallSelf(bool is_machine, bool send_uninstall_ping) {
  CORE_LOG(L2, (_T("[UninstallSelf]")));
  Setup setup(is_machine);
  return setup.Uninstall(send_uninstall_ping);
}

}  // namespace install_self

}  // namespace omaha
