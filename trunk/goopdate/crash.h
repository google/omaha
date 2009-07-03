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

#ifndef OMAHA_GOOPDATE_CRASH_H__
#define OMAHA_GOOPDATE_CRASH_H__

#include <windows.h>
#include <dbghelp.h>
#include <atlsecurity.h>
#include <atlstr.h>
#include <map>
#include "base/basictypes.h"
#include "omaha/goopdate/const_goopdate.h"
#include "third_party/gtest/include/gtest/gtest_prod.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/src/client/windows/crash_generation/crash_generation_server.h"
#include "third_party/breakpad/src/client/windows/handler/exception_handler.h"

namespace omaha {

// Annotates the version reported along with the crash.
const TCHAR* const kCrashVersionPostfixString =
  _T("")
#if !OFFICIAL_BUILD
  _T(".private")
#endif
#if DEBUG
  _T(".debug")
#endif
#if UNITTEST
  _T(".ut")
#endif
;  // NOLINT

// Official builds can only send a few crashes per day. Debug builds including
// all build modes for unit tests send unlimited number of crashes.
const int kCrashReportMaxReportsPerDay =
#if OFFICIAL_BUILD && !defined(UNITTEST)
                                         5;
#else
                                         0x7fffffff;
#endif

const TCHAR* const kNoCrashHandlerEnvVariableName =
    _T("GOOGLE_UPDATE_NO_CRASH_HANDLER");

// TODO(omaha): refactor so this is not a static class.
// TODO(omaha): rename class name to better indicate its funtionality.
class Crash {
 public:
  typedef std::map<std::wstring, std::wstring> ParameterMap;

  // Installs and uninstalls Breakpad exception handler. Calling the
  // functions from DllMain results in undefined behavior, including
  // deadlocks.
  static HRESULT InstallCrashHandler(bool is_machine);
  static void UninstallCrashHandler();

  // Starts the server to listen for out-of-process crashes.
  static HRESULT StartServer();

  // Stops the crash server.
  static void StopServer();

  // Generates a divide by zero to trigger Breakpad dump in non-ship builds.
  static int CrashNow();

  // Handles out-of-process crash requests.
  static HRESULT CrashHandler(bool is_machine,
                              const google_breakpad::ClientInfo& client_info,
                              const CString& crash_filename);

  // Reports a crash, uploads it is out of process or can_upload_in_process is
  // true, saves a copy of the crash, and deletes the crash file.
  static HRESULT Report(bool can_upload_in_process,
                        const CString& crash_filename,
                        const CString& custom_info_filename,
                        const CString& lang);

  // Sets a version string which is appended to the 'ver' parameter sent
  // with the crash report.
  static void set_version_postfix(const TCHAR* version_postfix) {
    version_postfix_ = version_postfix;
  }

  // Sets the guid to be reported. If not set, it uses the machine/user guid.
  static void set_guid(const TCHAR* guid) { guid_ = guid; }

  // Sets how many reports can be sent until the crash report sender starts
  // rejecting and discarding crashes.
  static void set_max_reports_per_day(int max_reports_per_day) {
    max_reports_per_day_ = max_reports_per_day;
  }

  static void set_crash_report_url(const TCHAR* crash_report_url) {
    crash_report_url_ = crash_report_url;
  }

  static bool is_machine() { return is_machine_; }

 private:

  static HRESULT Initialize(bool is_machine);

  // Reports a crash of Google Update. Does not delete the crash file.
  static HRESULT ReportGoogleUpdateCrash(bool can_upload,
                                         const CString& crash_filename,
                                         const CString& custom_info_filename,
                                         const CString& lang);

  // Reports an out-of-process crash on behalf of another product. Does not
  // delete the crash file.
  static HRESULT ReportProductCrash(bool can_upload,
                                    const CString& crash_filename,
                                    const CString& custom_info_filename,
                                    const CString& lang);

  // Initializes the crash directory. Creates the directory if it does not
  // exist.
  static HRESULT InitializeCrashDir();

  static HRESULT InitializeDirSecurity(CString* dir);

  // Returns true if the current process is reporting an exception.
  static HRESULT IsCrashReportProcess(bool* is_crash_report_process);

  // Logs an entry in the Windows Event Log for the specified source.
  static HRESULT Log(uint16 type,
                     uint32 id,
                     const TCHAR* source,
                     const TCHAR* description);

  // Starts the sender process with the environment variables setup such that
  // the sender process doesn't register crash filter to avoid potential
  // recursive crashes problem.
  static HRESULT StartSenderWithCommandLine(CString* cmd_line);

  // Creates a text file that contains name/value pairs of custom information.
  // The text file is created in the same directory as the given dump file, but
  // with a .txt extension. Stores the path of the text file created in the
  // custom_info_filepath parameter.
  // TODO(omaha): Move this functionality to breakpad. All the information
  // needed to write custom information file is known when the dump is generated
  // and hence breakpad could as easily create the text file.
  static HRESULT CreateCustomInfoFile(
      const CString& dump_file,
      const google_breakpad::CustomClientInfo& client_info,
      CString* custom_info_filepath);

  // Sends a crash report. If sent successfully, report_id contains the
  // report id generated by the crash server.
  static HRESULT DoSendCrashReport(bool can_upload,
                                   bool is_out_of_process,
                                   const CString& crash_filename,
                                   const ParameterMap& parameters,
                                   CString* report_id);

  // Callback function to run after the minidump has been written.
  static bool MinidumpCallback(const wchar_t* dump_path,
                               const wchar_t* minidump_id,
                               void* context,
                               EXCEPTION_POINTERS* exinfo,
                               MDRawAssertionInfo* assertion,
                               bool succeeded);

  // Returns true if the crash has happened in an Omaha process which
  // has a top level window up.
  static bool IsInteractive();

  // Returns the "prod" product name if found in the map or a default,
  // constant string otherwise.
  static CString GetProductName(const ParameterMap& parameters);

  // Updates the crash metrics after uploading the crash.
  static void UpdateCrashUploadMetrics(bool is_out_of_process, HRESULT hr);

  // Uploads the crash, logs the result of the crash upload, and updates
  // the crash metrics.
  static HRESULT UploadCrash(bool is_out_of_process,
                             const CString& crash_filename,
                             const ParameterMap& parameters,
                             CString* report_id);

  // Creates a back up copy of the current crash for future debugging use cases.
  static HRESULT SaveLastCrash(const CString& crash_filename,
                               const CString& product_name);

  // Cleans up stale crashes from the crash dir. Curently, crashes older than
  // 1 day are deleted.
  static HRESULT CleanStaleCrashes();

  // Retrieves the minidump exception information from the minidump file.
  static HRESULT GetExceptionInfo(const CString& crash_filename,
                                  MINIDUMP_EXCEPTION* ex_info);

  // Receives a top-level window and sets the param to true if the window
  // belongs to this process.
  static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM param);

  // Callback function to run when an exception is passing through an
  // exception barrier.
  static void __stdcall EBHandler(EXCEPTION_POINTERS* ptrs);

  // Callback function to run when a new client connects to the crash server.
  static void _cdecl ClientConnectedCallback(
      void* context,
      const google_breakpad::ClientInfo* client_info);

  // Callback function to run when a client signals a crash to the crash server.
  static void _cdecl ClientCrashedCallback(
      void* context,
      const google_breakpad::ClientInfo* client_info,
      const std::wstring* dump_path);

  // Callback function to run when a client disconnects from the crash server.
  static void _cdecl ClientExitedCallback(
      void* context,
      const google_breakpad::ClientInfo* client_info);

  // Builds a security attribute to allow all users to connect to the
  // crash server named pipe, when the server is running as system.
  static bool BuildPipeSecurityAttributes(CSecurityAttributes* sa);

  // Builds a security attribute to allow full control for the Local System
  // account and read/execute for the Administrators group, when the crash
  // handler is running as Local System.
  static bool BuildCrashDirSecurityAttributes(CSecurityAttributes* sa);

  // TODO(omaha): fix static instances of class type not allowed.
  static CString module_filename_;
  static CString Crash::crash_dir_;
  static CString Crash::checkpoint_file_;
  static CString version_postfix_;
  static CString guid_;
  static CString crash_report_url_;
  static int max_reports_per_day_;
  static google_breakpad::ExceptionHandler* exception_handler_;
  static google_breakpad::CrashGenerationServer* crash_server_;

  static bool is_machine_;

  static const int kCrashReportAttempts       = 3;
  static const int kCrashReportResendPeriodMs = 1 * 60 * 60 * 1000;  // 1 hour.

  // Default string to report out-of-process crashes with in the case
  // 'prod' information is not available.
  static const TCHAR* const kDefaultProductName;

  friend class CrashTest;

  FRIEND_TEST(CrashTest, CleanStaleCrashes);
  FRIEND_TEST(CrashTest, CreateCustomInfoFile);
  FRIEND_TEST(CrashTest, GetExceptionInfo);
  FRIEND_TEST(CrashTest, GetProductName);
  FRIEND_TEST(CrashTest, InstallCrashHandler);
  FRIEND_TEST(CrashTest, IsCrashReportProcess);
  FRIEND_TEST(CrashTest, Report_OmahaCrash);
  FRIEND_TEST(CrashTest, Report_ProductCrash);
  FRIEND_TEST(CrashTest, SaveLastCrash);
  FRIEND_TEST(CrashTest, StartServer);
  FRIEND_TEST(CrashTest, WriteMinidump);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Crash);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_CRASH_H__

