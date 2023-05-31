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

#include "omaha/common/crash_utils.h"

#include <atlbase.h>
#include <atlsecurity.h>
#include <atlstr.h>
#include <string.h>
#include <map>
#include <vector>

#include "omaha/base/app_util.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/environment_block_modifier.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/user_info.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "third_party/breakpad/src/client/windows/common/ipc_protocol.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

using google_breakpad::CustomClientInfo;
using google_breakpad::CustomInfoEntry;

namespace omaha {

namespace crash_utils {

namespace {

// TODO(omaha): Avoid having a class type as a static variable.
static CString g_crash_version_postfix_string = kDefaultCrashVersionPostfix;

HRESULT InitializeCrashDirSecurity(CString *crash_dir) {
  ASSERT1(crash_dir);

  // Users can only read permissions on the crash dir.
  CDacl dacl;
  const uint8 kAceFlags = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  if (!dacl.AddAllowedAce(Sids::System(), GENERIC_ALL, kAceFlags) ||
      !dacl.AddAllowedAce(Sids::Admins(), GENERIC_ALL, kAceFlags) ||
      !dacl.AddAllowedAce(Sids::Users(), READ_CONTROL, kAceFlags)) {
    return GOOPDATE_E_CRASH_SECURITY_FAILED;
  }

  SECURITY_INFORMATION si = DACL_SECURITY_INFORMATION |
                            PROTECTED_DACL_SECURITY_INFORMATION;
  DWORD error = ::SetNamedSecurityInfo(crash_dir->GetBuffer(),
                                       SE_FILE_OBJECT,
                                       si,
                                       NULL,
                                       NULL,
                                       const_cast<ACL*>(dacl.GetPACL()),
                                       NULL);
  crash_dir->ReleaseBuffer();

  if (error != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

}  // namespace

HRESULT InitializeCrashDir(bool is_machine, CString *crash_dir_out) {
  ConfigManager* cm = ConfigManager::Instance();
  CString dir = is_machine ? cm->GetMachineCrashReportsDir() :
                             cm->GetUserCrashReportsDir();

  // Use the temporary directory of the process if the crash directory can't be
  // initialized for any reason. Users can't read files in other users' temp
  // directories, so the temp dir is a good option to still have crash handling.
  if (dir.IsEmpty()) {
    dir = app_util::GetTempDir();
  }

  if (dir.IsEmpty()) {
    return GOOPDATE_E_CRASH_NO_DIR;
  }

  if (crash_dir_out) {
    *crash_dir_out = dir;
  }

  return S_OK;
}

HRESULT GetCrashPipeName(CString* pipe_name) {
  ASSERT1(pipe_name);

  // Append the current user's sid to the pipe name, so that machine and
  // user instances of the crash server open different pipes.
  CString user_sid;
  HRESULT hr = user_info::GetProcessUser(NULL, NULL, &user_sid);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[GetCrashPipeName][GetProcessUser failed][0x%08x]"), hr));
    return hr;
  }

  *pipe_name = kCrashPipeNamePrefix;
  SafeCStringAppendFormat(pipe_name, _T("\\%s"), user_sid);
#ifdef _WIN64
  pipe_name->Append(kObjectName64Suffix);
#endif

  return S_OK;
}

HRESULT AddPipeSecurityDaclToDesc(bool is_machine, CSecurityDesc* sd) {
  ASSERT1(sd);

  // Get the default DACL for this process owner.

  CAccessToken current_token;
  if (!current_token.GetEffectiveToken(TOKEN_QUERY)) {
    HRESULT hr = HRESULTFromLastError();
    OPT_LOG(LE, (_T("[Failed to get current thread token][0x%08x]"), hr));
    return FAILED(hr) ? hr : E_FAIL;
  }

  CDacl dacl;
  if (!current_token.GetDefaultDacl(&dacl)) {
    HRESULT hr = HRESULTFromLastError();
    OPT_LOG(LE, (_T("[Failed to get default DACL][0x%08x]"), hr));
    return FAILED(hr) ? hr : E_FAIL;
  }

  // If we're running as user, using the default security descriptor for the
  // pipe will grant full control to LocalSystem, administrators, and the
  // creator owner; we can just use that as-is.

  if (!is_machine) {
    sd->SetDacl(dacl);
    return S_OK;
  }

  // If we're running as machine, we need to add some custom attributes to
  // the pipe to allow all users on the local machine to to connect to it.

  const ACCESS_MASK kPipeAccessMask = FILE_READ_ATTRIBUTES  |
                                      FILE_READ_DATA        |
                                      FILE_WRITE_ATTRIBUTES |
                                      FILE_WRITE_DATA       |
                                      SYNCHRONIZE;
  if (!dacl.AddAllowedAce(ATL::Sids::Users(), kPipeAccessMask)) {
    OPT_LOG(LE, (_T("[Failed to setup pipe security]")));
    return E_FAIL;
  }
  if (!dacl.AddDeniedAce(ATL::Sids::Network(), FILE_ALL_ACCESS)) {
    OPT_LOG(LE, (_T("[Failed to setup pipe security]")));
    return E_FAIL;
  }

  sd->SetDacl(dacl);
  return S_OK;
}

HRESULT BuildPipeSecurityAttributes(bool is_machine, CSecurityDesc* sd) {
  ASSERT1(sd);
  ASSERT1(!sd->GetPSECURITY_DESCRIPTOR());

  // If we're on Vista or later, start with the low integrity SACL, so that we
  // can accept connections from browser plugins.
  HRESULT hr = vista_util::SetMandatorySacl(MandatoryLevelLow, sd);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to set low integrity SACL][%#x]"), hr));
    return hr;
  }

  hr = AddPipeSecurityDaclToDesc(is_machine, sd);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Failed to add pipe security DACL][%#x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT GetCustomInfoFilePath(const CString& dump_file,
                              CString* custom_info_filepath) {
  *custom_info_filepath = GetPathRemoveExtension(dump_file);
  SafeCStringAppendFormat(custom_info_filepath, _T(".txt"));
  return S_OK;
}

HRESULT CreateCustomInfoFile(const CString& dump_file,
                             const CustomInfoMap& custom_info_map,
                             CString* custom_info_filepath) {
  UTIL_LOG(L1, (_T("[CreateCustomInfoFile][%s][%d pairs]"),
                dump_file, custom_info_map.size()));

  // Since goopdate_utils::WriteNameValuePairsToFile is implemented in terms
  // of WritePrivateProfile API, relative paths are relative to the Windows
  // directory instead of the current directory of the process.
  ASSERT1(!::PathIsRelative(dump_file));

  // Determine the path for custom info file.
  GetCustomInfoFilePath(dump_file, custom_info_filepath);

  HRESULT hr = goopdate_utils::WriteNameValuePairsToFile(*custom_info_filepath,
                                                         kCustomClientInfoGroup,
                                                         custom_info_map);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[WriteNameValuePairsToFile failed][%s][0x%#x]"),
                  *custom_info_filepath, hr));
    return hr;
  }

  return S_OK;
}

HRESULT OpenCustomInfoFile(const CString& custom_info_file_name,
                           HANDLE* custom_info_file_handle) {
  UTIL_LOG(L1, (_T("[OpenCustomInfoFile][%s]"), custom_info_file_name));
  ASSERT1(!::PathIsRelative(custom_info_file_name));

  // Determine the path for custom info file.
  CString custom_info_filepath("");
  GetCustomInfoFilePath(custom_info_file_name, &custom_info_filepath);

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.bInheritHandle = TRUE;
  scoped_hfile file_handle(::CreateFile(
      custom_info_filepath.GetString(),
      GENERIC_WRITE,
      0,
      &security_attributes,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL));
  if (!file_handle) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[CreateFile failed][%s][0x%#x]"),
        custom_info_filepath, hr));
    return hr;
  }

  *custom_info_file_handle = release(file_handle);
  return S_OK;
}

HRESULT WriteCustomInfoFile(HANDLE custom_info_file_handle,
                            const CustomInfoMap& custom_info_map) {
  UTIL_LOG(L1, (_T("[WriteCustomInfoFile][%d pairs]"),
                custom_info_map.size()));
  HRESULT hr = goopdate_utils::WriteNameValuePairsToHandle(
      custom_info_file_handle,
      kCustomClientInfoGroup,
      custom_info_map);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[WriteNameValuePairsToHandle failed][0x%#x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT StartProcessWithNoExceptionHandler(CString* cmd_line) {
  OPT_LOG(LW, (_T("[StartProcessWithNoExceptionHandler][%s]"), *cmd_line));

  // Add an environment variable to the crash reporter process to indicate it
  // not to install a crash handler. This avoids an infinite loop in the case
  // the reporting process crashes also. When the reporting process begins
  // execution, the presence of this environment variable is tested, and the
  // crash handler will not be installed.
  EnvironmentBlockModifier env_block_mod;
  env_block_mod.SetVar(kNoCrashHandlerEnvVariableName, _T("1"));
  std::vector<TCHAR> new_env_block;
  if (!env_block_mod.CreateForCurrentUser(&new_env_block)
      || new_env_block.empty()) {
    return HRESULTFromLastError();
  }

  STARTUPINFO si = {sizeof(si)};
  PROCESS_INFORMATION pi = {0};
  if (!::CreateProcess(NULL,
                       cmd_line->GetBuffer(),
                       NULL,
                       NULL,
                       false,
                       CREATE_UNICODE_ENVIRONMENT,
                       &new_env_block.front(),
                       NULL,
                       &si,
                       &pi)) {
    return HRESULTFromLastError();
  }

  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);
  return S_OK;
}

HRESULT StartCrashReporter(bool is_interactive,
                           bool is_machine,
                           const CString& crash_filename,
                           const CString* custom_info_filename_opt) {
  ASSERT1(!crash_filename.IsEmpty());
  ASSERT1(!custom_info_filename_opt || !custom_info_filename_opt->IsEmpty());

  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_is_interactive_set(is_interactive);
  builder.set_is_machine_set(is_machine);
  builder.set_crash_filename(crash_filename);
  if (custom_info_filename_opt && !custom_info_filename_opt->IsEmpty()) {
    builder.set_custom_info_filename(*custom_info_filename_opt);
  }

  const CString exe_path = goopdate_utils::BuildGoogleUpdateExePath(is_machine);
  ASSERT1(!exe_path.IsEmpty());

  CString cmd_line = builder.GetCommandLine(exe_path);
  return StartProcessWithNoExceptionHandler(&cmd_line);
}

// Checks for the presence of an environment variable. We are not interested
// in the value of the variable but only in its presence.
HRESULT IsCrashReportProcess(bool* is_crash_report_process) {
  ASSERT1(is_crash_report_process);
  if (::GetEnvironmentVariable(kNoCrashHandlerEnvVariableName, NULL, 0)) {
    *is_crash_report_process = true;
    return S_OK;
  } else {
    DWORD error(::GetLastError());
    *is_crash_report_process = false;
    return error == ERROR_ENVVAR_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(error);
  }
}

// CustomClientInfo contains an array of CustomInfoEntry structs, which contain
// two wchar_t arrays. These are expected to be null-terminated, and should be
// if the client uses the setter functions in CustomInfoEntry -- however, this
// is read from the other process (which is suspect), and needs to be validated
// before we use it in this process.
HRESULT ConvertCustomClientInfoToMap(const CustomClientInfo& client_info,
                                     CustomInfoMap* map_out) {
  ASSERT1(map_out);

  map_out->clear();
  if (!client_info.count || !client_info.entries) {
    // If either field is 0, return an empty map.
    return S_OK;
  }

  HRESULT hr = S_OK;
  for (size_t i = 0; i < client_info.count; ++i) {
    const CustomInfoEntry& entry = client_info.entries[i];
    CString safename;
    CString safevalue;

    // We use wcsnlen here since Breakpad uses wchar_t throughout.  If CString
    // was ever a narrow type, we'd need to call ::WideCharToMultiByte.
    size_t namelen = _tcsnlen(entry.name, CustomInfoEntry::kNameMaxLength);
    safename.SetString(entry.name, static_cast<int>(namelen));
    size_t valuelen = _tcsnlen(entry.value, CustomInfoEntry::kValueMaxLength);
    safevalue.SetString(entry.value, static_cast<int>(valuelen));

    if (namelen == CustomInfoEntry::kNameMaxLength ||
        valuelen == CustomInfoEntry::kValueMaxLength) {
      // Not zero terminated!
      hr = S_FALSE;
    }

    const bool already_exists = map_out->find(safename) != map_out->end();
    (*map_out)[safename] = safevalue;
    UTIL_LOG(L6, (_T("[ConvertCustomClientInfoToMap][%s][%s][%d]"),
                  safename, safevalue, already_exists));
  }

  return hr;
}

bool IsUploadDeferralRequested(const CustomInfoMap& custom_info_map) {
  for (CustomInfoMap::const_iterator it = custom_info_map.begin();
       it != custom_info_map.end(); ++it) {
    if (it->first.CompareNoCase(kDeferUploadCustomFieldName) != 0) {
      continue;
    }
    if (it->second.CompareNoCase(kDeferUploadCustomFieldValue) == 0) {
      return true;
    }
  }

  return false;
}

CString GetCrashVersionString() {
  // Note: Assumes that we're being called from goopdate, or from some other
  // code path that has initialized the omaha_version library.
  CString version(GetVersionString());
  version.Append(g_crash_version_postfix_string);
  return version;
}

void SetCrashVersionPostfix(const CString& new_postfix) {
  g_crash_version_postfix_string = new_postfix;
}

}  // namespace crash_utils

}  // namespace omaha
