// Copyright 2004-2009 Google Inc.
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
// Tool that subscribes to the Omaha-hosted Breakpad crash handler and then
// intentionally crashes itself.

#include <windows.h>
#include <wtypes.h>
#include <tchar.h>
#include <atlbase.h>
#include <atlstr.h>
#include <vector>
#include "omaha/base/constants.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/error.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"

#pragma warning(push)
#pragma warning(disable: 4826)
#include "third_party/breakpad/src/client/windows/handler/exception_handler.h"
#pragma warning(pop)

class BreakpadConnection {
 public:
  BreakpadConnection() : eh_(NULL) {}

  ~BreakpadConnection() {
    Disconnect();
  }

  HRESULT Connect(bool is_machine) {
    CString pipe_name;
    HRESULT hr = BuildCrashPipeName(is_machine, &pipe_name);
    if (FAILED(hr)) {
      return hr;
    }
    _tprintf(_T("- Breakpad pipe name: \"%s\"\n"), pipe_name.GetString());

    // Note: Since we bail out if the EH tries to run in-process, it doesn't
    // matter if we can write to this directory.  It just needs to be valid.
    const CString crash_dir = omaha::GetCurrentDir();

    PopulateCustomInfoEntries();
    google_breakpad::CustomClientInfo custom_client_info = {
        &custom_entries_->front(), custom_entries_->size()
    };

    const int kHandlerTypes = google_breakpad::ExceptionHandler::HANDLER_ALL;
    const MINIDUMP_TYPE kDumpType = MiniDumpWithFullMemory;
    eh_ = new google_breakpad::ExceptionHandler(crash_dir.GetString(),
                                                NULL,   // filter
                                                NULL,   // callback
                                                NULL,   // callback_context
                                                kHandlerTypes,
                                                kDumpType,
                                                pipe_name,
                                                &custom_client_info);

    if (NULL == eh_) {
      _tprintf(_T("*** Couldn't create ExceptionHandler!  Aborting.\n"));
      return E_POINTER;
    }

    if (!eh_->IsOutOfProcess()) {
      _tprintf(_T("*** ExceptionHandler tried to run in-process!  Aborting.")
               _T(" (Check that ") CRASH_HANDLER_NAME _T(".exe is running.)\n"));
      Disconnect();
      return E_UNEXPECTED;
    }

    _tprintf(_T("- Exception handler installed!\n"));
    return S_OK;
  }

  void Disconnect() {
    if (eh_) {
      delete eh_;
      eh_ = NULL;
    }
  }

#pragma warning(push)
#pragma warning(disable : 4723)   // C4723: potential divide by 0
  int CrashUsingDivByZero() {
    volatile int foo = 10;
    volatile int bar = foo - 10;
    volatile int baz = foo / bar;
    return baz;
  }
#pragma warning(pop)

 private:
  google_breakpad::ExceptionHandler* eh_;
  std::vector<google_breakpad::CustomInfoEntry>* custom_entries_;

  HRESULT BuildCrashPipeName(bool is_machine, CString* pipe_name) {
    CString user_sid;

    if (is_machine) {
      user_sid = omaha::kLocalSystemSid;
    } else {
      HRESULT hr = omaha::user_info::GetProcessUser(NULL, NULL, &user_sid);
      if (FAILED(hr)) {
        _tprintf(_T("*** Couldn't get user SID (0x%08lx)\n"), hr);
        return hr;
      }
    }

    omaha::SafeCStringFormat(pipe_name, _T("%s\\%s"),
                             omaha::kCrashPipeNamePrefix, user_sid);

#ifdef _WIN64
    pipe_name->Append(omaha::kObjectName64Suffix);
#endif

    return S_OK;
  }

  void PopulateCustomInfoEntries() {
    custom_entries_ = new std::vector<google_breakpad::CustomInfoEntry>;
    custom_entries_->push_back(google_breakpad::CustomInfoEntry(
        _T("prod"), _T("CrashHandlerClient")));
    custom_entries_->push_back(google_breakpad::CustomInfoEntry(
        _T("ver"), _T("4.3.2.1")));
    custom_entries_->push_back(google_breakpad::CustomInfoEntry(
        _T("lang"), _T("en")));
    custom_entries_->push_back(google_breakpad::CustomInfoEntry(
        _T("interesting_text"), _T("hello_world")));
  }

  DISALLOW_COPY_AND_ASSIGN(BreakpadConnection);
};

bool ParseCommandLine(int argc, TCHAR* argv[], bool *is_machine_out) {
  switch (argc) {
    case 0:
      return false;

    case 1:
      // Default to user crash handler.
      *is_machine_out = false;
      return true;

    case 2:
      if (0 == _tcsicmp(argv[1], _T("machine"))) {
        *is_machine_out = true;
        return true;
      }
      else if (0 == _tcsicmp(argv[1], _T("user"))) {
        *is_machine_out = false;
        return true;
      }
      return false;

    default:
      return false;
  }
}

int _tmain(int argc, TCHAR* argv[]) {
  bool is_machine = false;
  if (!ParseCommandLine(argc, argv, &is_machine)) {
    _tprintf(_T("\n*** Usage: CrashHandlerClient [user | machine]\n"));
    return E_INVALIDARG;
  }

  BreakpadConnection breakpad;

  _tprintf(_T("\nPID: %lu\n"), GetCurrentProcessId());
  _tprintf(_T("Connecting to Crash Handler... (is_machine: %d)\n"), is_machine);

  HRESULT hr = breakpad.Connect(is_machine);
  if (FAILED(hr)) {
    return hr;
  }

  _tprintf(_T("Crashing...\n"));
  breakpad.CrashUsingDivByZero();

  _tprintf(_T("... That's odd, we're still here.  Removing crash handler.\n"));
  breakpad.Disconnect();

  return S_FALSE;
}

