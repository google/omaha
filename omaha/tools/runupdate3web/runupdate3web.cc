// Copyright 2021 Google Inc.
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
//
// A test tool for interacting with the Update3Web interfaces.

#include <atlbase.h>
#include <windows.h>

#include <vector>

#include "goopdate/omaha3_idl.h"
#include "omaha/base/debug.h"
#include "omaha/base/utils.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

enum Operation {
  CHECK_FOR_UPDATE = 0,
  DOWNLOAD = 1,
  INSTALL = 2,
  UPDATE = 3,
  LAUNCH_COMMAND = 4,
};

const size_t kLaunchCmdNumberOfParameters = 9;

}  // namespace

HRESULT InitializeBundle(bool is_machine, CComPtr<IAppBundleWeb>& bundle_web) {
  CComPtr<IGoogleUpdate3Web> update3web;
  HRESULT hr = update3web.CoCreateInstance(
      is_machine ? __uuidof(GoogleUpdate3WebMachineClass)
                 : __uuidof(GoogleUpdate3WebUserClass));
  if (FAILED(hr)) {
    wprintf(_T("Could not create COM instance [0x%x]\n"), hr);
    return hr;
  }

  CComPtr<IDispatch> bundle_dispatch;
  hr = update3web->createAppBundleWeb(&bundle_dispatch);
  if (FAILED(hr)) {
    wprintf(_T("createAppBundleWeb failed [0x%x]\n"), hr);
    return hr;
  }

  CComPtr<IAppBundleWeb> bundle;
  hr = bundle_dispatch.QueryInterface(&bundle);
  if (FAILED(hr)) {
    wprintf(_T("bundle_dispatch.QueryInterface failed [0x%x]\n"), hr);
    return hr;
  }

  hr = bundle->initialize();
  if (FAILED(hr)) {
    wprintf(_T("bundle->initialize failed [0x%x]\n"), hr);
    return hr;
  }

  bundle_web = bundle;
  return S_OK;
}

HRESULT DoLoopUntilDone(Operation operation, CComPtr<IAppBundleWeb> bundle) {
  bool done = false;
  while (!done) {
    if (!bundle) {
      wprintf(_T("No bundle is defined!\n"));
      return E_UNEXPECTED;
    }

    CComPtr<IDispatch> app_dispatch;
    HRESULT hr = bundle->get_appWeb(0, &app_dispatch);
    if (FAILED(hr)) {
      wprintf(_T("bundle->get_appWeb failed [0x%x]\n"), hr);
      return hr;
    }

    CComPtr<IAppWeb> app;
    hr = app_dispatch.QueryInterface(&app);
    if (FAILED(hr)) {
      wprintf(_T("app_dispatch.QueryInterface failed [0x%x]\n"), hr);
      return hr;
    }

    CComPtr<IDispatch> state_dispatch;
    hr = app->get_currentState(&state_dispatch);
    if (FAILED(hr)) {
      wprintf(_T("app->currentState failed [0x%x]\n"), hr);
      return hr;
    }

    CComPtr<ICurrentState> state;
    hr = state_dispatch.QueryInterface(&state);
    if (FAILED(hr)) {
      wprintf(_T("state_dispatch.QueryInterface failed [0x%x]\n"), hr);
      return hr;
    }

    CString stateDescription;
    CString extraData;

    LONG state_value = 0;
    hr = state->get_stateValue(&state_value);
    if (FAILED(hr)) {
      wprintf(_T("state->get_stateValue failed [0x%x]\n"), hr);
      return hr;
    }

    switch (state_value) {
      case STATE_INIT:
        stateDescription = _T("Initializating...");
        break;

      case STATE_WAITING_TO_CHECK_FOR_UPDATE:
      case STATE_CHECKING_FOR_UPDATE: {
        stateDescription = _T("Checking for update...");

        CComPtr<IDispatch> current_version_dispatch;
        hr = app->get_currentVersionWeb(&current_version_dispatch);
        if (FAILED(hr)) {
          wprintf(_T("app->get_currentVersionWeb failed [0x%x]\n"), hr);
          return hr;
        }

        CComPtr<IAppVersionWeb> current_version;
        hr = current_version_dispatch.QueryInterface(&current_version);
        if (FAILED(hr)) {
          wprintf(_T("current_version_dispatch.QueryInterface failed [0x%x]\n"),
                  hr);
          return hr;
        }

        CComBSTR version;
        hr = current_version->get_version(&version);
        if (FAILED(hr)) {
          wprintf(_T("current_version->get_version failed [0x%x]\n"), hr);
          return hr;
        }

        extraData.Format(_T("[Current Version][%s]"), version);
        break;
      }

      case STATE_UPDATE_AVAILABLE: {
        stateDescription = _T("Update available!");

        CComPtr<IDispatch> next_version_dispatch;
        hr = app->get_nextVersionWeb(&next_version_dispatch);
        if (FAILED(hr)) {
          wprintf(_T("app->get_nextVersionWeb failed [0x%x]\n"), hr);
          return hr;
        }

        CComPtr<IAppVersionWeb> next_version;
        hr = next_version_dispatch.QueryInterface(&next_version);
        if (FAILED(hr)) {
          wprintf(_T("next_version_dispatch.QueryInterface failed [0x%x]\n"),
                  hr);
          return hr;
        }

        CComBSTR version;
        hr = next_version->get_version(&version);
        if (FAILED(hr)) {
          wprintf(_T("next_version->get_version failed [0x%x]\n"), hr);
          return hr;
        }

        extraData.Format(_T("[Next Version][%s]"), version);
        if (operation == CHECK_FOR_UPDATE) {
          done = true;
          break;
        }

        hr = bundle->download();
        if (FAILED(hr)) {
          wprintf(_T("bundle->download failed [0x%x]\n"), hr);
          return hr;
        }

        break;
      }

      case STATE_WAITING_TO_DOWNLOAD:
      case STATE_RETRYING_DOWNLOAD:
        stateDescription = _T("Contacting server...");
        break;

      case STATE_DOWNLOADING: {
        stateDescription = _T("Downloading...");

        ULONG bytes_downloaded = 0;
        hr = state->get_bytesDownloaded(&bytes_downloaded);
        if (FAILED(hr)) {
          wprintf(_T("state->get_bytesDownloaded failed [0x%x]\n"), hr);
          return hr;
        }

        ULONG total_bytes_to_download = 0;
        hr = state->get_totalBytesToDownload(&total_bytes_to_download);
        if (FAILED(hr)) {
          wprintf(_T("state->get_totalBytesToDownload failed [0x%x]\n"), hr);
          return hr;
        }

        LONG download_time_remaining_ms = 0;
        hr = state->get_downloadTimeRemainingMs(&download_time_remaining_ms);
        if (FAILED(hr)) {
          wprintf(_T("state->get_downloadTimeRemainingMs failed [0x%x]\n"), hr);
          return hr;
        }

        extraData.Format(
            _T("[Bytes downloaded][%d][Bytes total][%d][Time remaining][%d]"),
            bytes_downloaded, total_bytes_to_download,
            download_time_remaining_ms);
        break;
      }

      case STATE_DOWNLOAD_COMPLETE:
      case STATE_EXTRACTING:
      case STATE_APPLYING_DIFFERENTIAL_PATCH:
      case STATE_READY_TO_INSTALL: {
        stateDescription = _T("Download completed!");

        CComPtr<IDispatch> current_version_dispatch;
        hr = app->get_currentVersionWeb(&current_version_dispatch);
        if (FAILED(hr)) {
          wprintf(_T("app->get_currentVersionWeb failed [0x%x]\n"), hr);
          return hr;
        }

        CComPtr<IAppVersionWeb> current_version;
        hr = current_version_dispatch.QueryInterface(&current_version);
        if (FAILED(hr)) {
          wprintf(_T("current_version_dispatch.QueryInterface failed [0x%x]\n"),
                  hr);
          return hr;
        }

        CComBSTR version;
        hr = current_version->get_version(&version);
        if (FAILED(hr)) {
          wprintf(_T("current_version->get_version failed [0x%x]\n"), hr);
          return hr;
        }

        ULONG bytes_downloaded = 0;
        hr = state->get_bytesDownloaded(&bytes_downloaded);
        if (FAILED(hr)) {
          wprintf(_T("state->get_bytesDownloaded failed [0x%x]\n"), hr);
          return hr;
        }

        ULONG total_bytes_to_download = 0;
        hr = state->get_totalBytesToDownload(&total_bytes_to_download);
        if (FAILED(hr)) {
          wprintf(_T("state->get_totalBytesToDownload failed [0x%x]\n"), hr);
          return hr;
        }

        extraData.Format(_T("[Current Version][%s]"), version);
        extraData.AppendFormat(_T("[Bytes downloaded][%d][Bytes total][%d]"),
                               bytes_downloaded, total_bytes_to_download);
        if (operation == DOWNLOAD) {
          done = true;
          break;
        }

        hr = bundle->install();
        if (FAILED(hr)) {
          wprintf(_T("bundle->install failed [0x%x]\n"), hr);
          return hr;
        }

        break;
      }

      case STATE_WAITING_TO_INSTALL:
      case STATE_INSTALLING: {
        stateDescription = _T("Installing...");

        LONG install_progress = 0;
        hr = state->get_installProgress(&install_progress);
        if (FAILED(hr)) {
          wprintf(_T("state->get_installProgress failed [0x%x]\n"), hr);
          return hr;
        }

        LONG install_time_remaining_ms = 0;
        hr = state->get_installTimeRemainingMs(&install_time_remaining_ms);
        if (FAILED(hr)) {
          wprintf(_T("state->get_installTimeRemainingMs failed [0x%x]\n"), hr);
          return hr;
        }

        extraData.Format(_T("[Install Progress][%d][Time remaining][%d]"),
                         install_progress, install_time_remaining_ms);
        break;
      }

      case STATE_INSTALL_COMPLETE:
        stateDescription = _T("Done!");
        done = true;
        break;

      case STATE_PAUSED:
        stateDescription = _T("Paused...");
        break;

      case STATE_NO_UPDATE:
        stateDescription = _T("No update available!");
        done = true;
        break;

      case STATE_ERROR: {
        stateDescription = _T("Error!");

        LONG error_code = 0;
        hr = state->get_errorCode(&error_code);
        if (FAILED(hr)) {
          wprintf(_T("state->get_errorCode failed [0x%x]\n"), hr);
          return hr;
        }

        CComBSTR completion_message;
        hr = state->get_completionMessage(&completion_message);
        if (FAILED(hr)) {
          wprintf(_T("state->get_completionMessage failed [0x%x]\n"), hr);
          return hr;
        }

        LONG installer_result_code = 0;
        hr = state->get_installerResultCode(&installer_result_code);
        if (FAILED(hr)) {
          wprintf(_T("state->get_installerResultCode failed [0x%x]\n"), hr);
          return hr;
        }

        extraData.Format(
            _T("[errorCode][%d][completionMessage][%s][installerResultCode][%")
            _T("d]"),
            error_code, completion_message, installer_result_code);
        done = true;
        break;
      }

      default:
        stateDescription = _T("Unhandled state...");
        break;
    }

    CString state_out;
    state_out.Format(_T("[State][%d][%s][%s]\n"), state_value, stateDescription,
                     extraData);
    wprintf(state_out);

    ::Sleep(100);
  }

  return S_OK;
}

HRESULT DoAppOperation(Operation operation, const CComBSTR& appid,
                       bool is_machine) {
  CComPtr<IAppBundleWeb> bundle;
  HRESULT hr = InitializeBundle(is_machine, bundle);
  if (FAILED(hr)) {
    wprintf(_T("InitializeBundle failed [0x%x]\n"), hr);
    return hr;
  }

  hr = (operation == CHECK_FOR_UPDATE || operation == UPDATE)
           ? bundle->createInstalledApp(appid)
           : bundle->createApp(appid, CComBSTR(_T("GPEZ")), CComBSTR(_T("en")),
                               CComBSTR(_T("")));
  if (FAILED(hr)) {
    wprintf(_T("App Creation failed [0x%x]\n"), hr);
    return hr;
  }

  hr = bundle->checkForUpdate();
  if (FAILED(hr)) {
    wprintf(_T("bundle->checkForUpdate failed [0x%x]\n"), hr);
    return hr;
  }

  return DoLoopUntilDone(operation, bundle);
}

HRESULT DoLaunchCommand(const CComBSTR& appid, bool is_machine,
                        const CComBSTR& command,
                        const std::vector<CComVariant>& argument_list) {
  CComPtr<IAppBundleWeb> bundle;
  HRESULT hr = InitializeBundle(is_machine, bundle);
  if (FAILED(hr)) {
    wprintf(_T("InitializeBundle failed [0x%x]\n"), hr);
    return hr;
  }

  hr = bundle->createInstalledApp(appid);
  if (FAILED(hr)) {
    wprintf(_T("bundle->createInstalledApp failed [0x%x]\n"), hr);
    return hr;
  }

  CComPtr<IDispatch> app_dispatch;
  hr = bundle->get_appWeb(0, &app_dispatch);
  if (FAILED(hr)) {
    wprintf(_T("bundle->appWeb failed [0x%x]\n"), hr);
    return hr;
  }

  CComPtr<IAppWeb> app;
  hr = app_dispatch.QueryInterface(&app);
  if (FAILED(hr)) {
    wprintf(_T("app_dispatch.QueryInterface failed [0x%x]\n"), hr);
    return hr;
  }

  CComPtr<IDispatch> cmd_dispatch;
  hr = app->get_command(command, &cmd_dispatch);
  if (FAILED(hr)) {
    wprintf(_T("app->command failed [0x%x]\n"), hr);
    return hr;
  }

  CComPtr<IAppCommandWeb> cmd;
  hr = cmd_dispatch.QueryInterface(&cmd);
  if (FAILED(hr)) {
    wprintf(_T("cmd_dispatch.QueryInterface failed [0x%x]\n"), hr);
    return hr;
  }

  wprintf(_T("Launching command.\n"));

  if (argument_list.size() != kLaunchCmdNumberOfParameters) {
    wprintf(_T("Invalid number of LaunchCommand arguments, needs to be %zd.\n"),
            kLaunchCmdNumberOfParameters);
    return E_INVALIDARG;
  }

  hr = cmd->execute(argument_list[0], argument_list[1], argument_list[2],
                    argument_list[3], argument_list[4], argument_list[5],
                    argument_list[6], argument_list[7], argument_list[8]);

  if (FAILED(hr)) {
    wprintf(_T("cmd->execute failed [0x%x]\n"), hr);
    return hr;
  }

  wprintf(_T("Command launched.\n"));

  while (true) {
    UINT status = 0;
    hr = cmd->get_status(&status);
    if (FAILED(hr)) {
      wprintf(_T("cmd->get_status failed [0x%x]\n"), hr);
      return hr;
    }

    CString stateDescription = _T("");

    switch (status) {
      case COMMAND_STATUS_RUNNING:
        stateDescription = _T("Running...");
        break;

      case COMMAND_STATUS_ERROR:
        stateDescription = _T("Error!");
        break;

      case COMMAND_STATUS_COMPLETE: {
        DWORD exit_code = 0;
        hr = cmd->get_exitCode(&exit_code);
        if (FAILED(hr)) {
          wprintf(_T("cmd->get_exitCode failed [0x%x]\n"), hr);
          return hr;
        }

        stateDescription.Format(_T("Exited with code %d."), exit_code);
        break;
      }

      default:
        stateDescription = _T("Unhandled state!");
        break;
    }

    CString state_out;
    state_out.Format(_T("[State][%d][%s]\n"), status, stateDescription);
    wprintf(state_out);

    if (status != COMMAND_STATUS_RUNNING) {
      return E_UNEXPECTED;
    }

    ::Sleep(100);
  }

  return hr;
}

bool ParseAndRun(int argc, TCHAR* argv[]) {
  ASSERT1(argc);
  ASSERT1(argv);

  scoped_co_init co_init;
  VERIFY1(SUCCEEDED(co_init.hresult()));

  if (argc < 4) {
    return false;
  }

  CString guid = argv[1];

  // Verify that the guid is valid.
  GUID parsed;
  if (FAILED(StringToGuidSafe(guid, &parsed))) {
    return false;
  }

  CComBSTR app_guid = argv[1];

  bool is_machine = !!_ttoi(argv[2]);
  Operation operation = static_cast<Operation>(_ttoi(argv[3]));

  switch (operation) {
    case CHECK_FOR_UPDATE:
    case DOWNLOAD:
    case INSTALL:
    case UPDATE:
      if (argc != 4) {
        return false;
      }
      DoAppOperation(operation, app_guid, is_machine);
      break;

    case LAUNCH_COMMAND: {
      if (argc < 5) {
        return false;
      }
      int command = _ttoi(argv[4]);
      std::vector<CComVariant> argument_list;
      if (argc > 5) {
        argument_list.insert(argument_list.begin(), argv + 5, argv + argc);
      }
      argument_list.resize(kLaunchCmdNumberOfParameters);

      DoLaunchCommand(app_guid, is_machine, command, argument_list);
      break;
    }

    default:
      return false;
  }

  return true;
}

}  // namespace omaha

int _tmain(int argc, TCHAR* argv[]) {
  if (!omaha::ParseAndRun(argc, argv)) {
    wprintf(
        _T("Usage: {GUID} ")
        _T("{is_machine: 0|1} ")
        _T("{0|1|2|3|4==CheckForUpdate|Download|Install|Update|LaunchCommand} ")
        _T("{command_id?}"));
    return -1;
  }

  return 0;
}

