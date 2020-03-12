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

// TODO(omaha): Reduce the amount of work we do in the inproc exception handler.
// We make several Win32 API calls and quite a few string allocations that we
// should probably avoid making while we're in an unreliable state.

#include "omaha/common/exception_handler.h"

#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/crash_utils.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/lang.h"

using google_breakpad::CustomClientInfo;
using google_breakpad::CustomInfoEntry;
using google_breakpad::ExceptionHandler;

namespace omaha {

using crash_utils::CustomInfoMap;

// static
HRESULT OmahaExceptionHandler::Create(bool is_machine,
  const CustomInfoMap& custom_info_map,
  std::unique_ptr<OmahaExceptionHandler>* handler_out) {
  CORE_LOG(L3, (_T("[OmahaExceptionHandler::Create][%d]"), is_machine));

  ASSERT1(OkayToInstall());
  ASSERT1(handler_out);
  if (!handler_out) {
    return E_POINTER;
  }

  std::unique_ptr<OmahaExceptionHandler> outp(new OmahaExceptionHandler(is_machine));
  HRESULT hr = outp->Initialize(custom_info_map);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[Initialize failed][%#08x]"), hr));
    return hr;
  }

  hr = outp->InstallHandler();
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[InstallHandler failed][%#08x]"), hr));
    return hr;
  }

  handler_out->reset(outp.release());
  return S_OK;
}

OmahaExceptionHandler::OmahaExceptionHandler(bool is_machine)
  : is_machine_(is_machine),
    is_internal_user_(ConfigManager::Instance()->IsInternalUser()) {
}

OmahaExceptionHandler::~OmahaExceptionHandler() {
  if (breakpad_exception_handler_.get()) {
    breakpad_exception_handler_.reset();
    CORE_LOG(L2, (_T("[exception handler has been uninstalled]")));
  }
}

HRESULT OmahaExceptionHandler::Initialize(
    const CustomInfoMap& custom_info_map) {
  // Create a directory to store crash dumps.
  HRESULT hr = crash_utils::InitializeCrashDir(is_machine_, &crash_dir_);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(!crash_dir_.IsEmpty());
  CORE_LOG(L2, (_T("[crash dir %s]"), crash_dir_));

  // Build our initial set of custom info entries.  If we're connected to an
  // out-of-process crash server, these will be uploaded alongside our minidump.
  AddCustomInfoEntry(_T("prod"), kCrashOmahaProductName);
  AddCustomInfoEntry(_T("ver"), crash_utils::GetCrashVersionString());
  AddCustomInfoEntry(_T("lang"), lang::GetDefaultLanguage(is_machine_));
  AddCustomInfoEntry(_T("guid"),
                     goopdate_utils::GetUserIdLazyInit(is_machine_));

  for (CustomInfoMap::const_iterator it = custom_info_map.begin();
       it != custom_info_map.end();
       ++it) {
    AddCustomInfoEntry(it->first, it->second);
  }

  return S_OK;
}

// Omaha will use OOP crash handling if the exception handler can be initialized
// for OOP, otherwise Omaha crashes will be handled in-process.
HRESULT OmahaExceptionHandler::InstallHandler() {
  CString pipe_name;
  VERIFY_SUCCEEDED(crash_utils::GetCrashPipeName(&pipe_name));
  UTIL_LOG(L6, (_T("[crash pipe][%s]"), pipe_name));

  // If the machine is internal to Google, include full memory in the minidump;
  // otherwise, do a normal minidump to respect PII.
  const MINIDUMP_TYPE dump_type = is_internal_user_ ?
                                      MiniDumpWithFullMemory : MiniDumpNormal;

  // Make sure that we have our custom info ready.  The CustomClientInfo struct
  // is copied during the ExceptionHandler constructor, so it can live on the
  // stack; however, the CustomInfoEntry array that it points to is not, and
  // it needs to exist for the life of the exception handler.
  ASSERT1(!custom_entries_.empty());
  CustomClientInfo custom_client_info = {
      &custom_entries_.front(), custom_entries_.size()
  };

  // Create the Breakpad exception handler.
  breakpad_exception_handler_.reset(
      new ExceptionHandler(crash_dir_.GetString(),
                           NULL,
                           &OmahaExceptionHandler::StaticMinidumpCallback,
                           reinterpret_cast<void*>(this),
                           ExceptionHandler::HANDLER_ALL,
                           dump_type,
                           pipe_name,
                           &custom_client_info));

  CORE_LOG(L2, (_T("[exception handler has been installed][out-of-process %d]"),
               IsOutOfProcess()));
  return S_OK;
}

void OmahaExceptionHandler::AddCustomInfoEntry(const CString& key,
                                               const CString& value) {
  // We can't add any new entries after the Breakpad handler has been created.
  ASSERT1(!breakpad_exception_handler_.get());

  custom_entries_.push_back(CustomInfoEntry(key, value));
}

// static
bool OmahaExceptionHandler::StaticMinidumpCallback(const wchar_t* dump_path,
                                                   const wchar_t* minidump_id,
                                                   void* context,
                                                   EXCEPTION_POINTERS*,
                                                   MDRawAssertionInfo*,
                                                   bool succeeded) {
  // If we're connected to an out-of-process crash handler, this callback will
  // still get called upon a crash, but dump_path and minidump_id will both be
  // NULL.
  if (context && succeeded &&
      dump_path && *dump_path &&
      minidump_id && *minidump_id) {
    OmahaExceptionHandler* thisptr =
        reinterpret_cast<OmahaExceptionHandler*>(context);
    thisptr->MinidumpCallback(dump_path, minidump_id);
  }

  // There are two ways to stop execution of the current process: ExitProcess
  // and TerminateProcess. Calling ExitProcess results in calling the
  // destructors of the static objects before the process exits.
  // TerminateProcess unconditionally stops the process so no user mode code
  // executes beyond this point.
  ::TerminateProcess(::GetCurrentProcess(),
                     static_cast<UINT>(GOOPDATE_E_CRASH));
  return true;
}

void OmahaExceptionHandler::MinidumpCallback(const wchar_t* dump_path,
                                             const wchar_t* minidump_id) const {
  // The implementation here must be as simple as possible -- use only the
  // resources needed to start a reporter process and then exit.

  ASSERT1(dump_path && *dump_path);
  ASSERT1(minidump_id && *minidump_id);

  // We need a way to see if the crash happens while we are installing
  // something. This is a tough spot to be doing anything at all since
  // we've been handling a crash.
  // TODO(omaha): redesign a better mechanism.
  bool is_interactive = IsInteractive();

  // TODO(omaha): format a command line without extra memory allocations.
  CString crash_filename;
  SafeCStringFormat(&crash_filename, _T("%s\\%s.dmp"),
                    dump_path, minidump_id);
  EnclosePath(&crash_filename);
  crash_utils::StartCrashReporter(is_interactive,
                                  is_machine_,
                                  crash_filename,
                                  NULL);

  // For in-proc crash generation, ExceptionHandler either creates a Normal
  // MiniDump, or a Full MiniDump, based on the dump_type. However, in the
  // case of OOP crash generation both the Normal and Full dumps are created
  // by the crash handling server, with the default full dump filename having
  // a suffix of "-full.dmp". If Omaha switches to using OOP crash generation,
  // this file is uploaded as well.
  if (is_internal_user_) {
    SafeCStringFormat(&crash_filename, _T("%s\\%s-full.dmp"),
                      dump_path, minidump_id);
    EnclosePath(&crash_filename);
    if (File::Exists(crash_filename)) {
      crash_utils::StartCrashReporter(is_interactive,
                                      is_machine_,
                                      crash_filename,
                                      NULL);
    }
  }
}

bool OmahaExceptionHandler::IsInteractive() const {
  bool result = false;
  ::EnumWindows(&OmahaExceptionHandler::EnumWindowsCallback,
                reinterpret_cast<LPARAM>(&result));
  return result;
}

// Finds if the given window is in the current process.
// static
BOOL CALLBACK OmahaExceptionHandler::EnumWindowsCallback(HWND hwnd,
                                                         LPARAM param) {
  DWORD pid = 0;
  ::GetWindowThreadProcessId(hwnd, &pid);
  if (::IsWindowVisible(hwnd) && pid == ::GetCurrentProcessId() && param) {
    *reinterpret_cast<bool*>(param) = true;
    return false;
  }
  return true;
}

// static
bool OmahaExceptionHandler::OkayToInstall() {
  bool is_crash_report_process = false;
  if (FAILED(crash_utils::IsCrashReportProcess(&is_crash_report_process))) {
    // If we can't decide, err towards not installing it.
    return false;
  }

  return !is_crash_report_process;
}

// static
int OmahaExceptionHandler::CrashNow() {
#ifdef DEBUG
#pragma warning(push)
#pragma warning(disable : 4723)   // C4723: potential divide by 0
  CORE_LOG(LEVEL_ERROR, (_T("[OmahaExceptionHandler::CrashNow]")));
  volatile int foo = 10;
  volatile int bar = foo - 10;
  volatile int baz = foo / bar;
  return baz;
#pragma warning(pop)
#else
  return 0;
#endif
}

}  // namespace omaha

