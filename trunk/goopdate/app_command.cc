// Copyright 2011 Google Inc.
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

#include "omaha/goopdate/app_command.h"

#include "base/scoped_ptr.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/exception_barrier.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/ping.h"
#include "omaha/common/ping_event.h"

namespace omaha {

namespace {

// Sends a single ping event to the Omaha server, synchronously.
void SendPing(const CString& app_guid,
              bool is_machine,
              const CString& session_id,
              PingEvent::Types type,
              PingEvent::Results result,
              int error_code,
              int extra_code) {
  PingEventPtr ping_event(new PingEvent(type, result, error_code, extra_code));

  Ping ping(is_machine, session_id, kCmdLineInstallSource_OneClick);
  std::vector<CString> apps;
  apps.push_back(app_guid);
  ping.LoadAppDataFromRegistry(apps);
  ping.BuildAppsPing(ping_event);
  ping.Send(true);  // true == is_fire_and_forget
}

// Waits on a process to exit, sends a ping based on the outcome.
// This is a COM object so that, during its lifetime, the process will not exit.
// The instance is AddRef'd in the instantiating thread and Release'd by the
// thread procedure when all work is completed.
class ATL_NO_VTABLE CompletePingSender
    : public CComObjectRootEx<CComMultiThreadModel>,
      public IUnknown {
 public:
  // Starts a wait on a process, belonging to the specified app and having the
  // given reporting ID. Will send a ping when the process exits.
  static void Start(const CString& app_guid,
                    bool is_machine,
                    const CString& session_id,
                    int reporting_id,
                    HANDLE process);

  BEGIN_COM_MAP(CompletePingSender)
  END_COM_MAP()

 protected:
  CompletePingSender();
  virtual ~CompletePingSender();

 private:
  static HRESULT Create(const CString& app_guid,
                        bool is_machine,
                        const CString& session_id,
                        int reporting_id,
                        HANDLE process,
                        CompletePingSender** sender);

  // Sends an EVENT_APP_COMMAND_COMPLETE ping with data from member
  // variables and parameters.
  void SendCompletePing(PingEvent::Results result, int error_code);

  // Waits for the process to exit, returning S_OK and the exit_code or the
  // underlying error code if the wait fails.
  HRESULT WaitForProcessExit(DWORD* exit_code);

  // Waits until the process exits or timeout occurs, then sends a ping with
  // the result. parameter is the CompletePingSender instance.
  static DWORD WINAPI WaitFunction(void* parameter);

  CString app_guid_;
  bool is_machine_;
  CString session_id_;
  int reporting_id_;
  scoped_process process_;

  DISALLOW_COPY_AND_ASSIGN(CompletePingSender);
};  // class CompletePingSender

CompletePingSender::CompletePingSender() {
}

HRESULT CompletePingSender::Create(const CString& app_guid,
                                   bool is_machine,
                                   const CString& session_id,
                                   int reporting_id,
                                   HANDLE process,
                                   CompletePingSender** sender) {
  ASSERT1(process && sender);

  scoped_process process_handle(process);
  process = NULL;

  typedef CComObject<CompletePingSender> ComObjectCompletePingSender;

  scoped_ptr<ComObjectCompletePingSender> new_object;
  HRESULT hr = ComObjectCompletePingSender::CreateInstance(address(new_object));
  if (FAILED(hr)) {
    return hr;
  }

  new_object->app_guid_ = app_guid;
  new_object->is_machine_ = is_machine;
  new_object->session_id_ = session_id;
  new_object->reporting_id_ = reporting_id;
  reset(new_object->process_, release(process_handle));

  new_object->AddRef();
  *sender = new_object.release();
  return S_OK;
}

CompletePingSender::~CompletePingSender() {
}

void CompletePingSender::Start(const CString& app_guid,
                               bool is_machine,
                               const CString& session_id,
                               int reporting_id,
                               HANDLE process) {
  ASSERT1(process);

  scoped_process process_handle(process);
  process = NULL;

  CComPtr<CompletePingSender> sender;
  HRESULT hr = CompletePingSender::Create(app_guid,
                                          is_machine,
                                          session_id,
                                          reporting_id,
                                          release(process_handle),
                                          &sender);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to create CompletePingSender]"),
                  _T("[0x%08x]"), hr));
    return;
  }

  void* context =
      reinterpret_cast<void *>(static_cast<CompletePingSender*>(sender));

  scoped_handle thread(::CreateThread(NULL, 0, WaitFunction, context, 0, NULL));

  if (thread) {
    // In case of success, the thread is responsible for calling Release.
    sender.Detach();
  } else {
    hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[failed to start wait thread for app command ")
                  _T("process exit]") _T("[0x%08x]"), hr));
    sender->SendCompletePing(PingEvent::EVENT_RESULT_ERROR, hr);
  }
}

void CompletePingSender::SendCompletePing(PingEvent::Results result,
                                          int error_code) {
  SendPing(app_guid_,
           is_machine_,
           session_id_,
           PingEvent::EVENT_APP_COMMAND_COMPLETE,
           result,
           error_code,
           reporting_id_);
}

HRESULT CompletePingSender::WaitForProcessExit(DWORD* exit_code) {
  ASSERT1(exit_code);
  if (!exit_code) {
    return E_INVALIDARG;
  }

  DWORD wait_result = ::WaitForSingleObject(get(process_), INFINITE);

  if (wait_result == WAIT_TIMEOUT) {
    return GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT;
  } else if (wait_result == WAIT_FAILED) {
    return HRESULTFromLastError();
  }

  ASSERT1(wait_result == WAIT_OBJECT_0);

  if (wait_result != WAIT_OBJECT_0) {
    return E_UNEXPECTED;
  }

  if (!::GetExitCodeProcess(get(process_), exit_code)) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

DWORD WINAPI CompletePingSender::WaitFunction(void* parameter) {
  scoped_co_init init_com_apt(COINIT_MULTITHREADED);
  HRESULT hr = init_com_apt.hresult();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[init_com_apt failed][0x%x]"), hr));
    return 0;
  }

  CComPtr<CompletePingSender> instance(
      reinterpret_cast<CompletePingSender*>(parameter));
  DWORD exit_code = 0;
  hr = instance->WaitForProcessExit(&exit_code);

  PingEvent::Results result = PingEvent::EVENT_RESULT_SUCCESS;
  int error_code = 0;

  if (FAILED(hr)) {
    result = PingEvent::EVENT_RESULT_ERROR;
    error_code = hr;
  } else {
    switch (exit_code) {
      case ERROR_SUCCESS_REBOOT_REQUIRED:
        result = PingEvent::EVENT_RESULT_SUCCESS_REBOOT;
        break;
      case ERROR_SUCCESS:
        result = PingEvent::EVENT_RESULT_SUCCESS;
        break;
      default:
        result = PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER;
        error_code = exit_code;
        break;
    }
  }

  instance->SendCompletePing(result, error_code);

  return 0;
}

// Attempts to read the command line from the given registry key and value.
// Logs a message in case of failure.
HRESULT ReadCommandLine(const CString& key_name,
                        const CString& value_name,
                        CString* command_line) {
  HRESULT hr = RegKey::GetValue(key_name, value_name, command_line);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to read command line]")
                  _T("[key %s][value %s][0x%08x]"), key_name, value_name, hr));
  }

  return hr;
}

// Checks if the specified value exists in the registry under the specified key.
// If so, attempts to read the value's DWORD contents into 'paramter'. Succeeds
// iff the value is absent or a DWORD value is successfully read.
HRESULT ReadCommandParameter(const CString& key_name,
                             const CString& value_name,
                             DWORD* parameter) {
  if (!RegKey::HasValue(key_name, value_name)) {
    return S_OK;
  }

  HRESULT hr = RegKey::GetValue(key_name, value_name, parameter);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to read command parameter]")
                  _T("[key %s][value %s][0x%08x]"), key_name, value_name, hr));
  }

  return hr;
}

}  // namespace

AppCommand::AppCommand(const CString& app_guid,
                       bool is_machine,
                       const CString& cmd_id,
                       const CString& cmd_line,
                       bool sends_pings,
                       const CString& session_id,
                       bool is_web_accessible,
                       DWORD reporting_id)
  : app_guid_(app_guid),
    is_machine_(is_machine),
    cmd_id_(cmd_id),
    session_id_(session_id),
    cmd_line_(cmd_line),
    sends_pings_(sends_pings),
    reporting_id_(reporting_id),
    is_web_accessible_(is_web_accessible) {
}

HRESULT AppCommand::Load(const CString& app_guid,
                         bool is_machine,
                         const CString& cmd_id,
                         const CString& session_id,
                         AppCommand** app_command) {
  ASSERT1(app_command);

  CString cmd_line;
  DWORD sends_pings = 0;
  DWORD is_web_accessible = 0;
  DWORD reporting_id = 0;

  ConfigManager* config_manager = ConfigManager::Instance();
  CString clients_key_name = config_manager->registry_clients(is_machine);

  CString app_key_name(AppendRegKeyPath(clients_key_name, app_guid));
  CString command_key_name(
      AppendRegKeyPath(app_key_name, kCommandsRegKeyName, cmd_id));

  // Prefer the new layout, otherwise look for the legacy layout. See comments
  // in app_command.h for description of each.
  if (!RegKey::HasKey(command_key_name)) {
    if (!RegKey::HasValue(app_key_name, cmd_id)) {
      return GOOPDATE_E_CORE_MISSING_CMD;
    }

    // Legacy command layout.
    HRESULT hr = ReadCommandLine(app_key_name, cmd_id, &cmd_line);
    if (FAILED(hr)) {
      return hr;
    }
  } else {
    // New command layout.
    HRESULT hr = ReadCommandLine(command_key_name, kRegValueCommandLine,
                                 &cmd_line);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ReadCommandParameter(command_key_name, kRegValueSendsPings,
                              &sends_pings);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ReadCommandParameter(command_key_name, kRegValueWebAccessible,
                              &is_web_accessible);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ReadCommandParameter(command_key_name, kRegValueReportingId,
                              &reporting_id);
    if (FAILED(hr)) {
      return hr;
    }
  }

  *app_command = new AppCommand(app_guid,
                                is_machine,
                                cmd_id,
                                cmd_line,
                                sends_pings != 0,
                                session_id,
                                is_web_accessible != 0,
                                reporting_id);
  return S_OK;
}

HRESULT AppCommand::Execute(HANDLE* process) const {
  ASSERT1(process);
  if (!process) {
    return E_INVALIDARG;
  }

  *process = NULL;

  CString cmd_line(cmd_line_);

  PROCESS_INFORMATION pi = {0};
  HRESULT hr = System::StartProcess(NULL, cmd_line.GetBuffer(), &pi);

  if (sends_pings_) {
    PingEvent::Results result = SUCCEEDED(hr) ?
        PingEvent::EVENT_RESULT_SUCCESS : PingEvent::EVENT_RESULT_ERROR;

    SendPing(app_guid_,
             is_machine_,
             session_id_,
             PingEvent::EVENT_APP_COMMAND_BEGIN,
             result,
             hr,
             reporting_id_);
  }

  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to launch cmd][%s][0x%08x]"), cmd_line_, hr));
    return hr;
  }

  ASSERT1(pi.hProcess);
  VERIFY1(::CloseHandle(pi.hThread));

  *process = pi.hProcess;

  if (sends_pings_) {
    StartBackgroundThread(pi.hProcess);
  }

  return S_OK;
}

// Starts a background thread with a duplicate of the process handle.
// We need to duplicate the handle because the original handle will be returned
// to the client.
void AppCommand::StartBackgroundThread(HANDLE command_process) const {
  HANDLE duplicate_process = NULL;

  // This is a pseudo handle that need not be closed.
  HANDLE this_process_handle = ::GetCurrentProcess();

  if (::DuplicateHandle(this_process_handle, command_process,
                        this_process_handle, &duplicate_process,
                        NULL, false, DUPLICATE_SAME_ACCESS)) {
    CompletePingSender::Start(app_guid_,
                              is_machine_,
                              session_id_,
                              reporting_id_,
                              duplicate_process);
  } else {
    CORE_LOG(LE, (_T("[failed call to DuplicateHandle][0x%08x]"),
                  HRESULTFromLastError()));
    SendPing(app_guid_,
             is_machine_,
             session_id_,
             PingEvent::EVENT_APP_COMMAND_COMPLETE,
             PingEvent::EVENT_RESULT_ERROR,
             HRESULTFromLastError(),
             reporting_id_);
  }
}

}  // namespace omaha
