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

#ifndef OMAHA_COMMON_EXCEPTION_HANDLER_H_
#define OMAHA_COMMON_EXCEPTION_HANDLER_H_

#include <windows.h>
#include <dbghelp.h>
#include <atlsecurity.h>
#include <atlstr.h>
#include <map>
#include <memory>

#include "base/basictypes.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/crash_utils.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/src/client/windows/crash_generation/crash_generation_server.h"

#pragma warning(push)
// C4826 Conversion from 'type_1' to 'type_2' is sign-extended.
#pragma warning(disable: 4826)
#include "third_party/breakpad/src/client/windows/handler/exception_handler.h"
#pragma warning(pop)

namespace omaha {

using crash_utils::CustomInfoMap;

class OmahaExceptionHandler {
 public:
  static HRESULT Create(bool is_machine,
                        const CustomInfoMap& custom_info_map,
                        std::unique_ptr<OmahaExceptionHandler>* handler_out);

  ~OmahaExceptionHandler();

  bool is_machine() const { return is_machine_; }

  bool IsOutOfProcess() const {
    if (breakpad_exception_handler_.get()) {
      return breakpad_exception_handler_->IsOutOfProcess();
    }
    return false;
  }

  void SetVersionPostfix(const CString& postfix);

  // Generates a divide by zero to trigger Breakpad dump in non-ship builds.
  static int CrashNow();

  // Returns true if it's okay to install the Breakpad exception handler in
  // this process.  The negation of crash_utils::IsCrashReportProcess().
  static bool OkayToInstall();

 private:
  explicit OmahaExceptionHandler(bool is_machine);

  // Creates the minidump storage directory, finds the copy of Omaha we'll use
  // to upload crash reports, and fills out custom_entries_.
  HRESULT Initialize(const CustomInfoMap& custom_info_map);

  // Initializes the Breakpad exception handler.
  HRESULT InstallHandler();

  // Adds an key/value pair to custom_entries_.
  void AddCustomInfoEntry(const CString& key, const CString& value);

  // Callback function to run after the minidump has been written.
  static bool StaticMinidumpCallback(const wchar_t* dump_path,
                                     const wchar_t* minidump_id,
                                     void* context,
                                     EXCEPTION_POINTERS* exinfo,
                                     MDRawAssertionInfo* assertion,
                                     bool succeeded);

  // Member worker function called by StaticMinidumpCallback.
  void MinidumpCallback(const wchar_t* dump_path,
                        const wchar_t* minidump_id) const;

  // Returns true if the crash has happened in an Omaha process which
  // has a top level window up.
  bool IsInteractive() const;

  // Receives a top-level window and sets the param to true if the window
  // belongs to this process.  Used by IsInteractive().
  static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM param);

  bool is_machine_;
  bool is_internal_user_;
  CString version_postfix_string_;
  CString crash_dir_;

  std::unique_ptr<google_breakpad::ExceptionHandler> breakpad_exception_handler_;
  std::vector<google_breakpad::CustomInfoEntry> custom_entries_;

  friend class ExceptionHandlerTest;

  DISALLOW_IMPLICIT_CONSTRUCTORS(OmahaExceptionHandler);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_EXCEPTION_HANDLER_H_

