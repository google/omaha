// Copyright 2008-2009 Google Inc.
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

#include "omaha/tools/omahacompatibility/compatibility_test.h"
#include <Windows.h>
#include <tchar.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/apply_tag.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/system.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/tools/omahacompatibility/common/ping_observer.h"
#include "omaha/tools/omahacompatibility/httpserver/http_server.h"
#include "omaha/tools/omahacompatibility/httpserver/update_check_handler.h"
#include "omaha/tools/omahacompatibility/httpserver/download_handler.h"

namespace omaha {

const TCHAR* const kUpdateCheckUrlPath = _T("/service/update2");
const TCHAR* const kDownloadUrlPath = _T("/download");
const DWORD kAuCheckPeriodMs = 5 * 60 * 1000;
const TCHAR* const kHost = _T("localhost");
const TCHAR* const kUrl = _T("http://localhost:8001/");
const TCHAR* const kDownloadUrl = _T("http://localhost:8001/download");
const TCHAR* const kUpdateCheckUrl =
    _T("http://localhost:8001/service/update2");
const int kPort = 8001;

int CompatibilityTest::Main(bool test_omaha) {
  CORE_LOG(L1, (_T("[Main]")));

  if (IsOmahaInstalled()) {
    printf("\nOmaha is already installed on this machine.\n");
    printf("The test may not work correctly.\n");
    printf("Ensure that the version of omaha being pointed to\n");
    printf("is atleast >= the registered versions reported above.\n");
    printf("Are you sure you want to run this test (Y/N)?");

    int response = getchar();
    if (response == 'n' || response == 'N') {
      return -1;
    }
  }

  // Read the config file.
  HRESULT hr = ReadConfigFile(config_file_,
                              kDownloadUrl,
                              &config_responses_);
  if (FAILED(hr)) {
    return -1;
  }

  // Create the observer with the guid.
  console_writer_.reset(new ConsoleWriter(
      GuidToString(config_responses_[0].guid)));

  // Setup the registry to make omaha talk to the local server.
  // TODO(omaha): Warn user about changing user settings.
  hr = SetupRegistry(config_responses_[0].needs_admin);
  if (FAILED(hr)) {
    return -1;
  }
  ON_SCOPE_EXIT(&CompatibilityTest::RestoreRegistry);

  printf("\nStarting the local http server.\n");
  hr = StartHttpServer();
  if (FAILED(hr)) {
    printf("\nLocal Http server start failed.\n");
    return -1;
  }

  if (test_omaha) {
    printf("\nStarting Googleupdate to install application.");
    printf("Please wait .....\n");
    printf("Omaha UI should show up in a sec.");
    printf("If it does not, please kill this process and restart test.\n");

    hr = StartGoogleUpdate();
    if (FAILED(hr)) {
      // TODO(omaha): Maybe report a verbose error message indicating
      // what could be wrong.
      printf("\nThe installation failed.\n");
      printf("Please refer to the omaha integration documentation\n");
      printf("for more information, it is possible that when you rerun\n");
      printf("you get a warning about omaha already running,\n");
      printf("please choose to continue in this case\n");
      return -1;
    }

    // TODO(omaha): Add a wait on the UI process, instead of depending on
    // the user correctly dismissing the UI.
    printf("\nThe installation completed successfully.\n");
    printf("Please dismiss the installer UI.\n");
    printf("Continue update test (Y/N)?");
    fflush(stdin);
    int response = getchar();
    if (response =='n' || response == 'N') {
      return -1;
    }

    hr = StartApplicationUpdate();
    if (FAILED(hr)) {
      printf("\nThe update failed.\n");
      printf("Please take a look at the omaha integration documentation\n");
      printf("and rerun the test. It is possible that when you rerun\n");
      printf("you might get a warning about omaha already running,\n");
      printf("please choose to continue in this case\n");
      return -1;
    }

    printf("\nUpdate succeeded. Wee.\n");
    printf("Congratulations on a successful install and update.\n");
    printf("Your friendly compatibility assistant will take your leave now.\n");
    printf("Bye\n");
  } else {
    while (true) {
      ::SleepEx(10000, false);
    }
  }

  return 0;
}

bool CompatibilityTest::IsOmahaInstalled() {
  bool is_omaha_installed = false;
  CString goopdate_key_name =
      ConfigManager::Instance()->registry_clients_goopdate(true);
  CString machine_version;
  HRESULT hr = RegKey::GetValue(goopdate_key_name, kRegValueProductVersion,
                                &machine_version);
  if (SUCCEEDED(hr)) {
    is_omaha_installed = true;
  }

  goopdate_key_name =
      ConfigManager::Instance()->registry_clients_goopdate(false);
  CString user_version;
  hr = RegKey::GetValue(goopdate_key_name,
                        kRegValueProductVersion,
                        &user_version);
  if (SUCCEEDED(hr)) {
    is_omaha_installed = true;
  }

  if (is_omaha_installed) {
    printf("\nOmaha Installed on machine!\n");
    if (!machine_version.IsEmpty()) {
      printf("Machine Omaha version: %s\n", CT2A(machine_version));
    }

    if (!user_version.IsEmpty()) {
      printf("User Omaha version: %s\n", CT2A(user_version));
    }
  }

  return is_omaha_installed;
}

// Starts the http server on a different thread.
HRESULT CompatibilityTest::StartHttpServer() {
  CORE_LOG(L1, (_T("[StartHttpServer]")));

  reset(thread_,
        ::CreateThread(NULL, 0,
                       &CompatibilityTest::StartStartHttpServerInternal,
                       this, 0, &thread_id_));
  return S_OK;
}

DWORD WINAPI CompatibilityTest::StartStartHttpServerInternal(
    void* param) {
  CompatibilityTest* omaha_compat =
      static_cast<CompatibilityTest*>(param);
  omaha_compat->RunHttpServerInternal();
  return 0;
}

HRESULT CompatibilityTest::RunHttpServerInternal() {
  CORE_LOG(L1, (_T("Starting local http server")));

  scoped_co_init init_com_apt(COINIT_MULTITHREADED);
  HRESULT hr = init_com_apt.hresult();
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[CoInitialize Failed.][0x%08x]"), hr));
    return hr;
  }

  HttpServer server(kHost, kPort);
  hr = server.Initialize();
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[HttpServer Initialize failed.][0x%08x]"), hr));
    return hr;
  }

  scoped_ptr<UpdateCheckHandler> update_handler(
      new UpdateCheckHandler(kUpdateCheckUrlPath, console_writer_.get()));
  hr = server.AddUrlHandler(update_handler.get());
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[Failed to add update handler.][0x%08x]"), hr));
    return hr;
  }

  scoped_ptr<DownloadHandler> download_handler(
      new DownloadHandler(kDownloadUrlPath));
  hr = server.AddUrlHandler(download_handler.get());
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[Failed to add download handler.][0x%08x]"), hr));
    return hr;
  }

  for (size_t i = 0; i < config_responses_.size(); ++i) {
    update_handler->AddAppVersionResponse(config_responses_[i]);
    download_handler->AddDownloadFile(config_responses_[i]);
  }

  update_handler.release();
  download_handler.release();
  server.Start();
  return S_OK;
}

HRESULT CompatibilityTest::RestoreRegistry(void) {
  CORE_LOG(L1, (_T("[RestoreRegistry]")));
  HRESULT hr = RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                                   kRegValueMonitorLastChecked);
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                           kRegValueNamePingUrl);
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                           kRegValueNameUrl);
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                           kRegValueAuCheckPeriodMs);
  if (FAILED(hr)) {
    return hr;
  }

  hr = RegKey::DeleteValue(MACHINE_REG_UPDATE_DEV,
                           kRegValueNameOverInstall);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT CompatibilityTest::SetupRegistry(bool needs_admin) {
  CORE_LOG(L1, (_T("[SetupRegistry]")));

  // Override the url, pingurl, AuCheckPeriod, overinstall.
  // Create the lastchecked value monitor.
  // Also create the last checked key, to allow a /ua launch
  // on deletion.
  HRESULT hr = RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                kRegValueMonitorLastChecked,
                                static_cast<DWORD>(1));
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[SetValue failed.][0x%08x]"), hr));
    return hr;
  }

  hr = RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                        kRegValueNamePingUrl,
                        kUpdateCheckUrl);
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[SetValue failed.][0x%08x]"), hr));
    return hr;
  }

  hr = RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                        kRegValueNameUrl,
                        kUpdateCheckUrl);
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[SetValue failed.][0x%08x]"), hr));
    return hr;
  }

  hr = RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                        kRegValueAuCheckPeriodMs,
                        kAuCheckPeriodMs);
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[SetValue failed.][0x%08x]"), hr));
    return hr;
  }

  hr = RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                        kRegValueNameOverInstall,
                        static_cast<DWORD>(1));
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[SetValue failed.][0x%08x]"), hr));
    return hr;
  }

  CString update_key_name =
      ConfigManager::Instance()->registry_update(needs_admin);
  hr = RegKey::SetValue(update_key_name,
                        kRegValueLastChecked,
                        static_cast<DWORD>(0));
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[SetValue failed.][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

// Builds the googleupdate file name to be run.
HRESULT CompatibilityTest::BuildTaggedGoogleUpdatePath() {
  CORE_LOG(L1, (_T("[BuildTaggedGoogleUpdatePath]")));

  GUID guid(GUID_NULL);
  HRESULT hr = ::CoCreateGuid(&guid);
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[CoCreateGuid failed 0x%08x]"), hr));
    return hr;
  }

  CString unique_exe(GuidToString(guid));
  unique_exe += _T(".exe");

  TCHAR temp_path[MAX_PATH] = {0};
  if (!::GetTempPath(MAX_PATH, temp_path)) {
    CORE_LOG(L1, (_T("[GetTempPath failed.]")));
    return HRESULTFromLastError();
  }

  tagged_google_update_path_ = ConcatenatePath(temp_path, unique_exe);
  return S_OK;
}

// Sets up the correct ap value to cause the server to send back
// an update response.
HRESULT CompatibilityTest::StartApplicationUpdate() {
  CORE_LOG(L1, (_T("[StartApplicationUpdate]")));

  // Set the ap value to "update_app".
  bool needs_admin = config_responses_[0].needs_admin;
  CString reg_key_name =
      ConfigManager::Instance()->registry_client_state(needs_admin);
  CString app_client_state_key_name = AppendRegKeyPath(
      reg_key_name,
      GuidToString(config_responses_[0].guid));
  HRESULT hr = RegKey::SetValue(app_client_state_key_name,
                                kRegValueAdditionalParams,
                                _T("update_app"));
  if (FAILED(hr)) {
    return hr;
  }

  // Delete the last checked value to force an update check.
  CString update_key_name =
      ConfigManager::Instance()->registry_update(needs_admin);
  hr = RegKey::DeleteValue(update_key_name, kRegValueLastChecked);
  if (FAILED(hr)) {
    return hr;
  }

  printf("\nSignaled Googleupdate to start application update.");
  printf("Waiting for events...\n");
  while (true) {
    ::SleepEx(1000, false);
    if (console_writer_->update_completed()) {
      PingEvent::Results result = console_writer_->update_result();
      if (result == PingEvent::EVENT_RESULT_SUCCESS) {
        CORE_LOG(L1, (_T("[Successfully completed update]")));
        return S_OK;
      } else {
        CORE_LOG(L1, (_T("[Update failed %d]"), result));
        return E_FAIL;
      }
    }
  }

  return S_OK;
}

// Stamps googleupdatesetup with the information from the config and
// runs it.
HRESULT CompatibilityTest::StartGoogleUpdate() {
  CORE_LOG(L1, (_T("[StartGoogleUpdate]")));

  // Stamp the googleupdatesetupe.exe with the information from
  // the config.
  HRESULT hr = BuildTaggedGoogleUpdatePath();
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[BuildTaggedGoogleUpdatePath failed 0x%08x]"), hr));
    return hr;
  }

  // For now we only read the information from the first config and use
  // that to stamp the binary.
  CString tag_str;
  tag_str.Format(
      _T("appguid=%s&appname=%s&needsadmin=%s&lang=%s"),
      GuidToString(config_responses_[0].guid),
      config_responses_[0].app_name,
      config_responses_[0].needs_admin ? _T("True") : _T("False"),
      config_responses_[0].language);

  ApplyTag tag;
  hr = tag.Init(googleupdate_setup_path_,
                CT2CA(tag_str),
                lstrlenA(CT2CA(tag_str)),
                tagged_google_update_path_,
                false);
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[ApplyTag.Init failed 0x%08x]"), hr));
    return hr;
  }

  hr = tag.EmbedTagString();
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[ApplyTag.EmbedTagString failed 0x%08x]"), hr));
    return hr;
  }

  // Start omaha to install the application.
  hr = System::ShellExecuteProcess(tagged_google_update_path_,
                                   NULL,
                                   NULL,
                                   NULL);
  if (FAILED(hr)) {
    CORE_LOG(L1, (_T("[Start installer failed 0x%08x]"), hr));
    return hr;
  }

  // Now wait for the install to complete. We poll the
  // observer for a install complete event.
  while (true) {
    ::SleepEx(1000, false);
    if (console_writer_->install_completed()) {
      PingEvent::Results result = console_writer_->install_result();
      if (result == PingEvent::EVENT_RESULT_SUCCESS) {
        CORE_LOG(L1, (_T("[Successfully completed install]")));
        return S_OK;
      } else {
        CORE_LOG(L1, (_T("[Start installer failed %d]"), result));
        return E_FAIL;
      }
    }
  }

  return S_OK;
}

}  // namespace omaha

