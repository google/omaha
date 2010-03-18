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

//
// Implementation of the Google Update recovery mechanism to be included in
// Google apps.

#include "omaha/common/google_update_recovery.h"
#include <shellapi.h>
#include <wininet.h>
#include <atlstr.h>
#include "omaha/common/const_addresses.h"
#include "omaha/common/signaturevalidator.h"
// TODO(omaha): Move this file to common.
#include "omaha/enterprise/const_group_policy.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

const int kRollbackWindowDays = 100;

const TCHAR* const kMachineRepairArgs = _T("/recover /machine");
const TCHAR* const kUserRepairArgs = _T("/recover");

// TODO(omaha): Add a Code Red lib version that is manually updated when
// we check in the lib.
const TCHAR* const kQueryStringFormat =
    _T("?appid=%s&appversion=%s&applang=%s&machine=%u")
    _T("&version=%s&osversion=%s&servicepack=%s");

// Information about where to obtain Omaha info.
// This must never change in Omaha.
const TCHAR* const kRegValueProductVersion  = _T("pv");
const TCHAR* const kRelativeGoopdateRegPath = _T("Software\\Google\\Update\\");
const TCHAR* const kRelativeClientsGoopdateRegPath =
    _T("Software\\Google\\Update\\Clients\\")
    _T("{430FD4D0-B729-4F61-AA34-91526481799D}");

// Starts another process via ::CreateProcess.
HRESULT StartProcess(const TCHAR* process_name, TCHAR* command_line) {
  if (!process_name && !command_line) {
    return E_INVALIDARG;
  }

  PROCESS_INFORMATION pi = {0};
  STARTUPINFO si = {sizeof(si), 0};

  // Feedback cursor is off while the process is starting.
  si.dwFlags = STARTF_FORCEOFFFEEDBACK;

  BOOL success = ::CreateProcess(
      process_name,     // Module name
      command_line,     // Command line
      NULL,             // Process handle not inheritable
      NULL,             // Thread handle not inheritable
      FALSE,            // Set handle inheritance to FALSE
      0,                // No creation flags
      NULL,             // Use parent's environment block
      NULL,             // Use parent's starting directory
      &si,              // Pointer to STARTUPINFO structure
      &pi);             // Pointer to PROCESS_INFORMATION structure

  if (!success) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);

  return S_OK;
}

// Check if a string starts with another string. Case-sensitive.
bool StringStartsWith(const TCHAR *str, const TCHAR *start_str) {
  if (!start_str || !str) {
    return false;
  }

  while (0 != *str) {
    // Check for matching characters
    TCHAR c1 = *str;
    TCHAR c2 = *start_str;

    // Reached the end of start_str?
    if (0 == c2)
      return true;

    if (c1 != c2)
      return false;

    ++str;
    ++start_str;
  }

  // If str is shorter than start_str, no match.  If equal size, match.
  return 0 == *start_str;
}

// Escape and unescape strings (shlwapi-based implementation).
// The intended usage for these APIs is escaping strings to make up
// URLs, for example building query strings.
//
// Pass false to the flag segment_only to escape the url. This will not
// cause the conversion of the # (%23), ? (%3F), and / (%2F) characters.

// Characters that must be encoded include any characters that have no
// corresponding graphic character in the US-ASCII coded character
// set (hexadecimal 80-FF, which are not used in the US-ASCII coded character
// set, and hexadecimal 00-1F and 7F, which are control characters),
// blank spaces, "%" (which is used to encode other characters),
// and unsafe characters (<, >, ", #, {, }, |, \, ^, ~, [, ], and ').
//
// The input and output strings can't be longer than INTERNET_MAX_URL_LENGTH

HRESULT StringEscape(const CString& str_in,
                     bool segment_only,
                     CString* escaped_string) {
  if (!escaped_string) {
    return E_INVALIDARG;
  }

  DWORD buf_len = INTERNET_MAX_URL_LENGTH + 1;
  HRESULT hr = ::UrlEscape(str_in,
                           escaped_string->GetBufferSetLength(buf_len),
                           &buf_len,
                           segment_only ?
                           URL_ESCAPE_PERCENT | URL_ESCAPE_SEGMENT_ONLY :
                           URL_ESCAPE_PERCENT);
  if (SUCCEEDED(hr)) {
    escaped_string->ReleaseBuffer();
  }
  return hr;
}

// Gets the temporary files directory for the current user.
// The directory returned may not exist.
// The returned path ends with a '\'.
// Fails if the path is longer than MAX_PATH.
HRESULT GetTempDir(CString* temp_path) {
  if (!temp_path) {
    return E_INVALIDARG;
  }

  temp_path->Empty();

  TCHAR buffer[MAX_PATH] = {0};
  DWORD num_chars = ::GetTempPath(MAX_PATH, buffer);
  if (!num_chars) {
    return HRESULT_FROM_WIN32(::GetLastError());
  } else if (num_chars >= MAX_PATH) {
    return E_FAIL;
  }

  *temp_path = buffer;
  return S_OK;
}

// Creates the specified directory.
HRESULT CreateDir(const CString& dir) {
  if (!::CreateDirectory(dir, NULL)) {
    DWORD error = ::GetLastError();
    if (ERROR_FILE_EXISTS != error && ERROR_ALREADY_EXISTS != error) {
      return HRESULT_FROM_WIN32(error);
    }
  }
  return S_OK;
}

HRESULT GetAndCreateTempDir(CString* temp_path) {
  if (!temp_path) {
    return E_INVALIDARG;
  }

  HRESULT hr = GetTempDir(temp_path);
  if (FAILED(hr)) {
    return hr;
  }
  if (temp_path->IsEmpty()) {
    return E_FAIL;
  }

  // Create this dir if it doesn't already exist.
  return CreateDir(*temp_path);
}


// Create a unique temporary file and returns the full path.
HRESULT CreateUniqueTempFile(const CString& user_temp_dir,
                             CString* unique_temp_file_path) {
  if (user_temp_dir.IsEmpty() || !unique_temp_file_path) {
    return E_INVALIDARG;
  }

  TCHAR unique_temp_filename[MAX_PATH] = {0};
  if (!::GetTempFileName(user_temp_dir,
                         _T("GUR"),  // prefix
                         0,          // form a unique filename
                         unique_temp_filename)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  *unique_temp_file_path = unique_temp_filename;
  if (unique_temp_file_path->IsEmpty()) {
    return E_FAIL;
  }

  return S_OK;
}

// Obtains the OS version and service pack.
HRESULT GetOSInfo(CString* os_version, CString* service_pack) {
  if (!os_version || !service_pack) {
    return E_INVALIDARG;
  }

  OSVERSIONINFO os_version_info = { 0 };
  os_version_info.dwOSVersionInfoSize = sizeof(os_version_info);
  if (!::GetVersionEx(&os_version_info)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    return hr;
  } else {
    os_version->Format(_T("%d.%d"),
                       os_version_info.dwMajorVersion,
                       os_version_info.dwMinorVersion);
    *service_pack = os_version_info.szCSDVersion;
  }
  return S_OK;
}

// Reads the specified string value from the specified registry key.
// Only supports value types REG_SZ and REG_EXPAND_SZ.
// REG_EXPAND_SZ strings are not expanded.
HRESULT GetRegStringValue(bool is_machine_key,
                          const CString& relative_key_path,
                          const CString& value_name,
                          CString* value) {
  if (!value) {
    return E_INVALIDARG;
  }

  value->Empty();
  HKEY root_key = is_machine_key ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  HKEY key = NULL;
  LONG res = ::RegOpenKeyEx(root_key, relative_key_path, 0, KEY_READ, &key);
  if (res != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(res);
  }

  // First get the size of the string buffer.
  DWORD type = 0;
  DWORD byte_count = 0;
  res = ::RegQueryValueEx(key, value_name, NULL, &type, NULL, &byte_count);
  if (ERROR_SUCCESS != res) {
    ::RegCloseKey(key);
    return HRESULT_FROM_WIN32(res);
  }
  if ((type != REG_SZ && type != REG_EXPAND_SZ) || (0 == byte_count)) {
    ::RegCloseKey(key);
    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
  }

  CString local_value;
  // GetBuffer throws when not able to allocate the requested buffer.
  TCHAR* buffer = local_value.GetBuffer(byte_count / sizeof(TCHAR));
  res = ::RegQueryValueEx(key,
                          value_name,
                          NULL,
                          NULL,
                          reinterpret_cast<BYTE*>(buffer),
                          &byte_count);
  ::RegCloseKey(key);
  if (ERROR_SUCCESS == res) {
    local_value.ReleaseBufferSetLength(byte_count / sizeof(TCHAR));
    *value = local_value;
  }

  return HRESULT_FROM_WIN32(res);
}

// Reads the specified DWORD value from the specified registry key.
// Only supports value types REG_DWORD.
// Assumes DWORD is sufficient buffer, which must be true for valid value type.
HRESULT GetRegDwordValue(bool is_machine_key,
                         const CString& relative_key_path,
                         const CString& value_name,
                         DWORD* value) {
  if (!value) {
    return E_INVALIDARG;
  }

  HKEY root_key = is_machine_key ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  HKEY key = NULL;
  LONG res = ::RegOpenKeyEx(root_key, relative_key_path, 0, KEY_READ, &key);
  if (res != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(res);
  }

  DWORD type = 0;
  DWORD byte_count = sizeof(*value);
  res = ::RegQueryValueEx(key,
                          value_name,
                          NULL,
                          &type,
                          reinterpret_cast<BYTE*>(value),
                          &byte_count);
  ::RegCloseKey(key);
  if (ERROR_SUCCESS != res) {
    return HRESULT_FROM_WIN32(res);
  }
  if ((type != REG_DWORD) || (0 == byte_count)) {
    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
  }

  return S_OK;
}

// Obtains information about the current Omaha installation.
// Attempts to obtain as much information as possible even if errors occur.
// Therefore, return values of GetRegStringValue are ignored.
HRESULT GetOmahaInformation(bool is_machine_app,
                            CString* omaha_version) {
  if (!omaha_version) {
    return E_INVALIDARG;
  }

  if (FAILED(GetRegStringValue(is_machine_app,
                               kRelativeClientsGoopdateRegPath,
                               kRegValueProductVersion,
                               omaha_version))) {
    *omaha_version = _T("0.0.0.0");
  }

  return S_OK;
}

// Builds the query portion of the recovery url.
// This method obtains values necessary to build the query that are not provided
// as parameters.
// Attempts to build with as much information as possible even if errors occur.
HRESULT BuildUrlQueryPortion(const CString& app_guid,
                             const CString& app_version,
                             const CString& app_language,
                             bool is_machine_app,
                             CString* query) {
  if (!query) {
    return E_INVALIDARG;
  }

  CString omaha_version;
  GetOmahaInformation(is_machine_app, &omaha_version);

  CString os_version;
  CString os_service_pack;
  GetOSInfo(&os_version, &os_service_pack);

  // All parameters must be escaped individually before building the query.
  CString app_guid_escaped;
  CString app_version_escaped;
  CString app_language_escaped;
  CString omaha_version_escaped;
  CString os_version_escaped;
  CString os_service_pack_escaped;
  StringEscape(app_guid, true, &app_guid_escaped);
  StringEscape(app_version, true, &app_version_escaped);
  StringEscape(app_language, true, &app_language_escaped);
  StringEscape(omaha_version, true, &omaha_version_escaped);
  StringEscape(os_version, true, &os_version_escaped);
  StringEscape(os_service_pack, true, &os_service_pack_escaped);

  query->Format(kQueryStringFormat,
                app_guid_escaped,
                app_version_escaped,
                app_language_escaped,
                is_machine_app ? 1 : 0,
                omaha_version_escaped,
                os_version_escaped,
                os_service_pack_escaped);

  return S_OK;
}

// Returns the full path to save the downloaded file to.
// The path is based on a unique temporary filename to avoid a conflict
// between multiple apps downloading to the same location.
// The path to this file is also returned. The caller is responsible for
// deleting the temporary file after using the download target path.
// If it cannot create the unique directory, it attempts to use the user's
// temporary directory and a constant filename.
HRESULT GetDownloadTargetPath(CString* download_target_path,
                              CString* temp_file_path) {
  if (!download_target_path || !temp_file_path) {
    return E_INVALIDARG;
  }

  CString user_temp_dir;
  HRESULT hr = GetAndCreateTempDir(&user_temp_dir);
  if (FAILED(hr)) {
    return hr;
  }

  hr = CreateUniqueTempFile(user_temp_dir, temp_file_path);
  if (SUCCEEDED(hr) && !temp_file_path->IsEmpty()) {
    *download_target_path = *temp_file_path;
    // Ignore the return value. A .tmp filename is better than none.
    download_target_path->Replace(_T(".tmp"), _T(".exe"));
  } else {
    // Try a static filename in the temp directory as a fallback.
    *download_target_path = user_temp_dir + _T("GoogleUpdateSetup.exe");
    *temp_file_path = _T("");
  }

  return S_OK;
}

HRESULT DownloadRepairFile(const CString& download_target_path,
                           const CString& app_guid,
                           const CString& app_version,
                           const CString& app_language,
                           bool is_machine_app,
                           DownloadCallback download_callback,
                           void* context) {
  CString query;
  BuildUrlQueryPortion(app_guid,
                       app_version,
                       app_language,
                       is_machine_app,
                       &query);

  CString url = omaha::kUrlCodeRedCheck + query;

  return download_callback(url, download_target_path, context);
}

// Makes sure the path is enclosed with double quotation marks.
void EnclosePath(CString* path) {
  if (path) {
    return;
  }

  if (!path->IsEmpty() && path->GetAt(0) != _T('"')) {
    path->Insert(0, _T('"'));
    path->AppendChar(_T('"'));
  }
}

HRESULT RunRepairFile(const CString& file_path, bool is_machine_app) {
  const TCHAR* repair_file_args = is_machine_app ? kMachineRepairArgs :
                                                   kUserRepairArgs;

  CString command_line(file_path);
  EnclosePath(&command_line);
  command_line.AppendChar(_T(' '));
  command_line.Append(repair_file_args);

  return StartProcess(NULL, command_line.GetBuffer());
}

}  // namespace

// Verifies the file's integrity and that it is signed by Google.
// We cannot prevent rollback attacks by using a version because the client
// may not be able to determine the current version if the files and/or
// registry entries have been deleted/corrupted.
// Therefore, we check that the file was signed recently.
HRESULT VerifyFileSignature(const CString& filename) {
  // Use Authenticode/WinVerifyTrust to verify the file.
  // Allow the revocation check to use the network.
  HRESULT hr = VerifySignature(filename, true);
  if (FAILED(hr)) {
    return hr;
  }

  // Verify that there is a Google certificate.
  if (!VerifySigneeIsGoogle(filename)) {
    return CERT_E_CN_NO_MATCH;
  }

  // Check that the file was signed recently to limit the window for
  // rollback attacks.
  return VerifyFileSignedWithinDays(filename, kRollbackWindowDays);
}

// Verifies the file contains the special markup resource for repair files.
HRESULT VerifyRepairFileMarkup(const CString& filename) {
  const TCHAR* kMarkupResourceName = MAKEINTRESOURCE(1);
  const TCHAR* kMarkupResourceType = _T("GOOGLEUPDATEREPAIR");
  const DWORD kMarkupResourceExpectedValue = 1;

  scoped_library module(::LoadLibraryEx(filename, 0, LOAD_LIBRARY_AS_DATAFILE));
  if (!module) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  HRSRC resource(::FindResource(get(module),
                                kMarkupResourceName,
                                kMarkupResourceType));
  if (!resource) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  if (sizeof(kMarkupResourceExpectedValue) !=
      ::SizeofResource(get(module), resource)) {
    return E_UNEXPECTED;
  }

  HGLOBAL loaded_resource(::LoadResource(get(module), resource));
  if (!loaded_resource) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  const DWORD* value = static_cast<DWORD*>(::LockResource(loaded_resource));
  if (!value) {
    return E_HANDLE;
  }

  if (kMarkupResourceExpectedValue != *value) {
    return E_UNEXPECTED;
  }

  return S_OK;
}

// Verifies the filename is not UNC name, the file exists, has a valid signature
// chain, is signed by Google, and contains the special markup resource for
// repair files.
HRESULT VerifyIsValidRepairFile(const CString& filename) {
  // Make sure file exists.
  if (!::PathFileExists(filename)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  HRESULT hr = VerifyFileSignature(filename);
  if (FAILED(hr)) {
    return hr;
  }

  return VerifyRepairFileMarkup(filename);
}

}  // namespace omaha

// If a repair file is run, the file will not be deleted until reboot. Delete
// after reboot will only succeed when executed by an admin or LocalSystem.
// Returns HRESULT_FROM_WIN32(ERROR_ACCESS_DISABLED_BY_POLICY) if automatic
// update checks are disabled.
HRESULT FixGoogleUpdate(const TCHAR* app_guid,
                        const TCHAR* app_version,
                        const TCHAR* app_language,
                        bool is_machine_app,
                        DownloadCallback download_callback,
                        void* context) {
  if (!app_guid || !app_version || !app_language || !download_callback) {
    return E_INVALIDARG;
  }

  DWORD update_check_period_override_minutes(UINT_MAX);
  HRESULT hr = omaha::GetRegDwordValue(
                   true,
                   GOOPDATE_POLICIES_RELATIVE,
                   omaha::kRegValueAutoUpdateCheckPeriodOverrideMinutes,
                   &update_check_period_override_minutes);
  if (SUCCEEDED(hr) && (0 == update_check_period_override_minutes)) {
    return HRESULT_FROM_WIN32(ERROR_ACCESS_DISABLED_BY_POLICY);
  }

  CString download_target_path;
  CString temp_file_path;
  hr = omaha::GetDownloadTargetPath(&download_target_path, &temp_file_path);
  if (FAILED(hr)) {
    return hr;
  }
  if (download_target_path.IsEmpty()) {
    hr = E_FAIL;
  }

  // After calling DownloadRepairFile, don't return until the repair file and
  // temp file have been deleted.
  hr = omaha::DownloadRepairFile(download_target_path,
                                 app_guid,
                                 app_version,
                                 app_language,
                                 is_machine_app,
                                 download_callback,
                                 context);

  if (SUCCEEDED(hr)) {
    hr = omaha::VerifyIsValidRepairFile(download_target_path);
  }

  if (FAILED(hr)) {
    ::DeleteFile(download_target_path);
    ::DeleteFile(temp_file_path);
    return hr;
  }

  hr = omaha::RunRepairFile(download_target_path, is_machine_app);
  ::MoveFileEx(download_target_path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
  ::DeleteFile(temp_file_path);

  return hr;
}
