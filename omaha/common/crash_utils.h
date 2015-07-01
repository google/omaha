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

// Common functions for configuring crash dump directories and the named pipes
// used for out-of-process crash handling.  These functions are called by
// GoogleCrashHandler and the in-process crash handler in Goopdate.

#ifndef OMAHA_COMMON_CRASH_UTILS_H_
#define OMAHA_COMMON_CRASH_UTILS_H_

#include <atlstr.h>
#include <map>

namespace google_breakpad {
struct CustomClientInfo;
}  // namespace google_breakpad

namespace omaha {

const TCHAR* const kDefaultCrashVersionPostfix =
#if !OFFICIAL_BUILD
  _T(".private")
#endif
#if DEBUG
  _T(".debug")
#endif
  _T("");

const TCHAR* const kNoCrashHandlerEnvVariableName =
    _T("GOOGLE_UPDATE_NO_CRASH_HANDLER");

const TCHAR* const kDeferUploadCustomFieldName =
    _T("deferred-upload");

const TCHAR* const kDeferUploadCustomFieldValue =
    _T("true");

// To change the upper limit for the number of crashes uploaded daily, define
// a registry value named "MaxCrashUploadsPerDay" under the UpdateDev key.
const int kDefaultCrashUploadsPerDay = 20;

namespace crash_utils {

typedef std::map<CString, CString> CustomInfoMap;

// Determines the directory to store minidumps in; attempts to create it and
// configure its permissions if it doesn't exist.
HRESULT InitializeCrashDir(bool is_machine, CString *crash_dir_out);

// Generates the name for the named pipe that the out-of-process crash handler
// uses to communicate with its clients.
HRESULT GetCrashPipeName(CString* pipe_name);

// Adds ACEs to a DACL allowing access from other users.
HRESULT AddPipeSecurityDaclToDesc(bool is_machine, CSecurityDesc* sd);

// Builds a security descriptor which allows user processes, including low
// integrity processes, to connect to the crash server's named pipe.
HRESULT BuildPipeSecurityAttributes(bool is_machine, CSecurityDesc* sd);

// Given a dump_file path determine the path to use for the custom client
// info file.
HRESULT GetCustomInfoFilePath(const CString& dump_file,
                              CString* custom_info_filepath);

// Creates a text file that contains name/value pairs of custom information.
// The text file is created in the same directory as the given dump file, but
// with a .txt extension. Stores the path of the text file created in the
// custom_info_filepath parameter.
// TODO(omaha): Move this functionality to breakpad. All the information
// needed to write custom information file is known when the dump is
// generated, and hence breakpad could just as easily create the text file.
HRESULT CreateCustomInfoFile(
    const CString& dump_file,
    const CustomInfoMap& custom_info_map,
    CString* custom_info_filepath);

// Opens a handle to the file which will be later used for custom info pairs.
// This allows a handle to be acquired prior to locking down file access for
// sandboxing.
HRESULT OpenCustomInfoFile(const CString& custom_info_file_name,
                           HANDLE* custom_info_file_handle);

// Write the custom data pairs to a file handle which was opened using
// OpenCustomInfoFile().
HRESULT WriteCustomInfoFile(HANDLE custom_info_file_handle,
                            const CustomInfoMap& custom_info_map);

// Starts a process with an environment variable defined which signals Goopdate
// to not use Breakpad crash handling. (This prevents mutual recursion between
// the crash handler and crash reporter.)
HRESULT StartProcessWithNoExceptionHandler(CString* cmd_line);

// Builds a command line for launching the crash reporter process, and launches
// the process using StartProcessWithNoExceptionHandler().  A valid path to a
// copy of Omaha and a minidump are required; a path to a custom info file is
// optional.
HRESULT StartCrashReporter(bool is_interactive,
                           bool is_machine,
                           const CString& crash_filename,
                           const CString* custom_info_filename_opt);

// Checks for the existence of the environment variable in a process created by
// StartSenderWithCommandLine(), above.
HRESULT IsCrashReportProcess(bool* is_crash_report_process);

// Converts a Breakpad CustomClientInfo struct to a map<CString,CString>.
// Returns S_OK on normal success.  If any entries in CustomClientInfo are not
// null terminated, their contents will be included.
HRESULT ConvertCustomClientInfoToMap(
    const google_breakpad::CustomClientInfo& client_info,
    CustomInfoMap* map_out);

// Returns true if the custom info map includes a key/value pair indicating
// that the client supports deferred crash upload and that we should hold on
// to this crash instead of uploading it immediately.
bool IsUploadDeferralRequested(const CustomInfoMap& custom_info);

// Returns the current version of Omaha as a string for use with the crash
// reporting server.  An postfix will be added to denote the build type.
CString GetCrashVersionString();

// Overrides the postfix used by GetCrashVersionString().
void SetCrashVersionPostfix(const CString& new_postfix);

}  // namespace crash_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_CRASH_UTILS_H_

