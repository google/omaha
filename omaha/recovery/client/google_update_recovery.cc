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

#include "omaha/recovery/client/google_update_recovery.h"
#include <lm.h>
#include <shellapi.h>
#include <wininet.h>
#include <atlstr.h>
#include "components/crx_file/crx_verifier.h"
#include "omaha/base/app_util.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/error.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/signaturevalidator.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/google_signaturevalidator.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

const int kRollbackWindowDays = 100;

const TCHAR* const kMachineRepairArgs = _T("/recover /machine");
const TCHAR* const kUserRepairArgs = _T("/recover");

// TODO(omaha): Add a Code Red lib version that is manually updated when
// we check in the lib.
const TCHAR* const kQueryStringFormat =
    _T("&appid=%s&appversion=%s&applang=%s&machine=%u")
    _T("&version=%s&userid=%s&osversion=%s&servicepack=%s");

// Information about where to obtain Omaha info.
// This must never change in Omaha.
const TCHAR* const kRegValueProductVersion  = _T("pv");
const TCHAR* const kRelativeClientsGoopdateRegPath =
    _T("Software\\") PATH_COMPANY_NAME _T("\\Update\\Clients\\")
    GOOPDATE_APP_ID;

// The UpdateDev registry value to override the Code Red url.
const TCHAR* const kRegValueNameCodeRedUrl = _T("CodeRedUrl");

// The hard-coded Recovery subdirectory where the CRX is unpacked and executed.
const TCHAR* const kRecoveryDirectory = _T("Recovery");

// The hard-coded SHA256 of the SubjectPublicKeyInfo used to sign the Recovery
// CRX which contains GoogleUpdateRecovery.exe.
std::vector<uint8_t> GetRecoveryCRXHash() {
  return std::vector<uint8_t>{0x5f, 0x94, 0xe0, 0x3c, 0x64, 0x30, 0x9f, 0xbc,
                              0xfe, 0x00, 0x9a, 0x27, 0x3e, 0x52, 0xbf, 0xa5,
                              0x84, 0xb9, 0xb3, 0x75, 0x07, 0x29, 0xde, 0xfa,
                              0x32, 0x76, 0xd9, 0x93, 0xb5, 0xa3, 0xce, 0x02};
}

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

// Create a unique temporary file under the provided parent directory and
// returns the full path.
HRESULT CreateUniqueTempFile(const CPath& parent_dir,
                             CPath* unique_temp_file_path) {
  ASSERT1(parent_dir.IsDirectory());
  ASSERT1(unique_temp_file_path);

  *unique_temp_file_path = CPath(GetTempFilenameAt(parent_dir, _T("GUR")));
  return unique_temp_file_path->FileExists() ? S_OK : HRESULTFromLastError();
}

// Create a unique temporary directory under the provided parent directory and
// returns the full path.
HRESULT CreateUniqueTempDir(const CPath& parent_dir, CPath* unique_temp_dir) {
  *unique_temp_dir = NULL;

  CPath temp_dir;
  HRESULT hr = CreateUniqueTempFile(parent_dir, &temp_dir);
  if (FAILED(hr)) {
    return hr;
  }

  ::DeleteFile(temp_dir);
  hr = CreateDir(temp_dir);
  if (FAILED(hr)) {
    return hr;
  }

  *unique_temp_dir += temp_dir;
  return S_OK;
}

// Resets and returns the Recovery directory under the currently executing
// module directory. For machine installs, this directory is under
// %ProgramFiles%. If the directory does not exist, it is created.
HRESULT ResetRecoveryDir(CPath* recovery_dir) {
  ASSERT1(recovery_dir);

  CString module_dir = app_util::GetCurrentModuleDirectory();
  if (module_dir.IsEmpty()) {
    return HRESULTFromLastError();
  }

  *recovery_dir = CPath(module_dir);
  *recovery_dir += kRecoveryDirectory;

  if (recovery_dir->IsDirectory()) {
    VERIFY_SUCCEEDED(DeleteDirectoryContents(*recovery_dir));
    return S_OK;
  }

  return CreateDir(*recovery_dir);
}

// Resets the Recovery directory and creates a temporary directory under the
// Recovery directory. For machine installs, this directory is under
// %ProgramFiles%.
HRESULT ResetRecoveryTempDir(CPath* temp_dir) {
  ASSERT1(temp_dir);

  CPath recovery_dir;
  HRESULT hr = ResetRecoveryDir(&recovery_dir);
  return SUCCEEDED(hr) ? CreateUniqueTempDir(recovery_dir, temp_dir) : hr;
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
    SafeCStringFormat(os_version, _T("%u.%u"),
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
                            CString* omaha_version,
                            CString* user_id) {
  if (!omaha_version || !user_id) {
    return E_INVALIDARG;
  }

  if (FAILED(GetRegStringValue(is_machine_app,
                               kRelativeClientsGoopdateRegPath,
                               kRegValueProductVersion,
                               omaha_version))) {
    *omaha_version = _T("0.0.0.0");
  }

  *user_id = goopdate_utils::GetUserIdLazyInit(is_machine_app);
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
  CString user_id;
  GetOmahaInformation(is_machine_app, &omaha_version, &user_id);

  CString os_version;
  CString os_service_pack;
  GetOSInfo(&os_version, &os_service_pack);

  // All parameters must be escaped individually before building the query.
  CString app_guid_escaped;
  CString app_version_escaped;
  CString app_language_escaped;
  CString omaha_version_escaped;
  CString user_id_escaped;
  CString os_version_escaped;
  CString os_service_pack_escaped;
  StringEscape(app_guid, true, &app_guid_escaped);
  StringEscape(app_version, true, &app_version_escaped);
  StringEscape(app_language, true, &app_language_escaped);
  StringEscape(omaha_version, true, &omaha_version_escaped);
  StringEscape(user_id, true, &user_id_escaped);
  StringEscape(os_version, true, &os_version_escaped);
  StringEscape(os_service_pack, true, &os_service_pack_escaped);

  SafeCStringFormat(query, kQueryStringFormat,
                    app_guid_escaped,
                    app_version_escaped,
                    app_language_escaped,
                    is_machine_app ? 1 : 0,
                    omaha_version_escaped,
                    user_id_escaped,
                    os_version_escaped,
                    os_service_pack_escaped);

  return S_OK;
}

// Returns the full path to save the downloaded file to and the corresponding
// parent directory.
HRESULT GetDownloadTargetPath(CPath* download_target_path,
                              CPath* parent_dir) {
  if (!download_target_path || !parent_dir) {
    return E_INVALIDARG;
  }

  HRESULT hr = ResetRecoveryTempDir(parent_dir);
  if (FAILED(hr)) {
    return hr;
  }

  *download_target_path = *parent_dir;
  *download_target_path += MAIN_EXE_BASE_NAME _T("Setup.crx3");
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

  CString url;
  HRESULT hr = GetRegStringValue(true,
                                 _T("SOFTWARE\\") PATH_COMPANY_NAME _T("\\UpdateDev"),
                                 kRegValueNameCodeRedUrl,
                                 &url);
  if (FAILED(hr)) {
    url = omaha::kUrlCodeRedCheck;
  }

  url += query;

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

// Verifies the integrity of the file, the file certificate, and the timestamp
// of the digital signature. Since the program
// files or the registry entries could be missing or corrupted, the version
// check can not be use to prevent rollback attacks. Use the timestamp instead.
HRESULT VerifyFileSignature(const CString& filename) {
  const bool allow_network_check = true;
  HRESULT hr = VerifyGoogleAuthenticodeSignature(filename, allow_network_check);
  if (FAILED(hr)) {
    return hr;
  }
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

// Validates the provided CRX using the |crx_hash|, and if validation succeeds,
// unpacks the CRX under |unpack_under_path|. Returns the unpacked EXE in
// |unpacked_exe|.
HRESULT ValidateAndUnpackCRX(const CPath& from_crx_path,
                             const crx_file::VerifierFormat& crx_format,
                             const std::vector<uint8_t>& crx_hash,
                             const CPath& unpack_under_path,
                             CPath* unpacked_exe) {
  ASSERT1(unpacked_exe);

  std::string public_key;
  if (crx_file::Verify(std::string(CT2A(from_crx_path)),
                       crx_format,
                       {crx_hash},
                       {},
                       &public_key,
                       NULL) != crx_file::VerifierResult::OK_FULL) {
    return CRYPT_E_NO_MATCH;
  }

  if (!crx_file::Crx3Unzip(from_crx_path, unpack_under_path)) {
    return E_UNEXPECTED;
  }

  CPath exe = unpack_under_path;
  exe += MAIN_EXE_BASE_NAME _T("Setup.exe");
  if (!exe.FileExists()) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  *unpacked_exe = exe;
  return S_OK;
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

  CPath download_target_path;
  CPath parent_dir;
  HRESULT hr = omaha::GetDownloadTargetPath(&download_target_path,
                                            &parent_dir);
  if (FAILED(hr)) {
    return hr;
  }

  hr = omaha::DownloadRepairFile(download_target_path,
                                 app_guid,
                                 app_version,
                                 app_language,
                                 is_machine_app,
                                 download_callback,
                                 context);

  if (FAILED(hr)) {
    return hr;
  }

  CPath unpacked_exe;
  hr = omaha::ValidateAndUnpackCRX(
           CPath(download_target_path),
           crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF,
           omaha::GetRecoveryCRXHash(),
           parent_dir,
           &unpacked_exe);
  if (FAILED(hr)) {
    return hr;
  }

  return omaha::RunRepairFile(unpacked_exe, is_machine_app);
}
