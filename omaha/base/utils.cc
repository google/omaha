// Copyright 2003-2010 Google Inc.
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

#include "omaha/base/utils.h"

#include <lm.h>
#include <mdmregistration.h>
#include <regstr.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <ATLComTime.h>
#include <atlpath.h>
#include <intsafe.h>
#include <stdint.h>
#include <limits>
#include <map>
#include <vector>

#include "omaha/base/app_util.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/const_config.h"
#include "omaha/base/const_timeouts.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/shell.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/system_info.h"
#include "omaha/base/time.h"
#include "omaha/base/user_info.h"
#include "omaha/base/user_rights.h"
#include "omaha/base/vistautil.h"

namespace omaha {

namespace {

// Private object namespaces for Vista processes.
const TCHAR* const kGoopdateBoundaryDescriptor = MAIN_EXE_BASE_NAME _T("_BD");
const TCHAR* const kGoopdatePrivateNamespace = MAIN_EXE_BASE_NAME;

// Helper for IsPrivateNamespaceAvailable().
// For simplicity, the handles opened here are leaked. We need these until
// process exit, at which point they will be cleaned up automatically by the OS.
bool EnsurePrivateNamespaceAvailable() {
  HANDLE boundary_descriptor =
      CreateBoundaryDescriptorWWrap(kGoopdateBoundaryDescriptor, 0);
  if (NULL == boundary_descriptor) {
    DWORD last_error(::GetLastError());
    UTIL_LOG(LE, (_T("CreateBoundaryDescriptor failed[%d]"), last_error));
    return false;
  }

  char sid[SECURITY_MAX_SID_SIZE] = {0};
  DWORD size = sizeof(sid);
  // Mark the boundary descriptor with the Admins Group SID. Consequently, all
  // admins, including SYSTEM, will create objects in the same private
  // namespace.
  if (!::CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sid, &size)) {
    UTIL_LOG(LE, (_T("[::CreateWellKnownSid failed][%d]"), ::GetLastError()));
    return false;
  }
  if (!AddSIDToBoundaryDescriptorWrap(&boundary_descriptor, sid)) {
    UTIL_LOG(LE, (_T("[AddSIDToBoundaryDescriptor failed][%d]"),
                  ::GetLastError()));
    return false;
  }

  NamedObjectAttributes attr;
  GetAdminDaclSecurityAttributes(&attr.sa, GENERIC_ALL);
  // The private namespace created here will be used to create objects of the
  // form "GoogleUpdate\xyz". As the article "Object Namespaces" on MSDN
  // explains, these kernel objects are safe from squatting attacks from lower
  // integrity processes.
  HANDLE namespace_handle =
      CreatePrivateNamespaceWWrap(&attr.sa,
                                  boundary_descriptor,
                                  kGoopdatePrivateNamespace);
  if (namespace_handle) {
    return true;
  }
  ASSERT(ERROR_ALREADY_EXISTS == ::GetLastError(),
         (_T("CreatePrivateNamespaceW failed: %d"), ::GetLastError()));

  // Another process has already created the namespace. Attempt to open.
  namespace_handle = OpenPrivateNamespaceWWrap(boundary_descriptor,
                                               kGoopdatePrivateNamespace);
  if (namespace_handle || ::GetLastError() == ERROR_DUP_NAME) {
    // ERROR_DUP_NAME indicates that we have called CreatePrivateNamespaceWWrap
    // or OpenPrivateNamespaceWWrap before in the same process. Either way, we
    // can now create objects prefixed with our private namespace.
    return true;
  }

  ASSERT(namespace_handle, (_T("[Could not open private namespace][%d]"),
                            ::GetLastError()));
  return false;
}

// Returns true if AddDllDirectory function is available, meaning
// LOAD_LIBRARY_SEARCH_* flags are available on the host system.
bool AreSearchFlagsAvailable() {
  // The LOAD_LIBRARY_SEARCH_* flags are available on systems that have
  // KB2533623 installed. To determine whether the flags are available, use
  // GetProcAddress to get the address of the AddDllDirectory,
  // RemoveDllDirectory, or SetDefaultDllDirectories function. If GetProcAddress
  // succeeds, the LOAD_LIBRARY_SEARCH_* flags can be used with LoadLibraryEx.
  static const auto add_dll_dir_func =
      reinterpret_cast<decltype(AddDllDirectory)*>(
          GetProcAddress(GetModuleHandle(_T("kernel32.dll")),
                                         "AddDllDirectory"));
  return !!add_dll_dir_func;
}

HMODULE LoadSystemLibraryHelper(const CString& library_path) {
  const DWORD kFlags = AreSearchFlagsAvailable()
      ? LOAD_LIBRARY_SEARCH_SYSTEM32
      : LOAD_WITH_ALTERED_SEARCH_PATH;
  return ::LoadLibraryExW(library_path.GetString(), nullptr, kFlags);
}

}  // namespace

// Returns 0 if an error occurs.
ULONGLONG VersionFromString(const CString& s) {
  int pos(0);
  unsigned int quad[4] = {0, 0, 0, 0};

  for (int i = 0; i < 4; ++i) {
    CString q = s.Tokenize(_T("."), pos);
    if (pos == -1) {
      return 0;
    }

    int quad_value(0);
    if (!String_StringToDecimalIntChecked(q, &quad_value)) {
      return 0;
    }

    quad[i] = static_cast<unsigned int>(quad_value);

    if (std::numeric_limits<uint16_t>::max() < quad[i]) {
      return 0;
    }
  }

  if (s.GetLength() + 1 != pos) {
    return 0;
  }

  return MAKEDLLVERULL(quad[0], quad[1], quad[2], quad[3]);
}

CString StringFromVersion(ULONGLONG version) {
  const WORD version_major = HIWORD(version >> 32);
  const WORD version_minor = LOWORD(version >> 32);
  const WORD version_build = HIWORD(version);
  const WORD version_patch = LOWORD(version);

  CString version_string;
  SafeCStringFormat(&version_string, _T("%u.%u.%u.%u"),
                    version_major,
                    version_minor,
                    version_build,
                    version_patch);
  return version_string;
}

CString GetCurrentDir() {
  TCHAR cur_dir[MAX_PATH] = {0};
  if (!::GetCurrentDirectory(MAX_PATH, cur_dir)) {
    return CString(_T('.'));
  }
  return CString(cur_dir);
}

HRESULT GetNewFileNameInDirectory(const CString& dir, CString* file_name) {
  ASSERT1(file_name);

  GUID guid = {0};
  HRESULT hr = ::CoCreateGuid(&guid);
  if (FAILED(hr)) {
    UTIL_LOG(LEVEL_WARNING, (_T("[CoCreateGuid failed][0x%08x]"), hr));
    return hr;
  }

  CString guid_file_name = GuidToString(guid);
  CPath file_path(dir);
  file_path.Append(guid_file_name);

  *file_name = static_cast<const TCHAR*>(file_path);
  return S_OK;
}

// determines if a time is in the distant past, present, or future
TimeCategory GetTimeCategory(const time64 system_time) {
  time64 now = GetCurrent100NSTime();

  // Times more than a few days in the future are wrong [I will allow a little
  // leeway, since it could be set in another future time zone, or a program
  // that likes UNC]]
  if (system_time > (now + kDaysTo100ns * 5)) {
    return FUTURE;
  }

  // times more than 40 years ago are wrong
  if (system_time < (now - kDaysTo100ns * 365 * 40)) {
    return PAST;
  }

  return PRESENT;
}

// Determine if a given time is probably valid
bool IsValidTime(const time64 t) {
  return (GetTimeCategory(t) == PRESENT);
}

LARGE_INTEGER MSto100NSRelative(DWORD ms) {
  const __int64 convert_ms_to_100ns_units = 1000 /*ms/us*/ * 10 /*us/100ns*/;
  __int64 timeout_100ns = static_cast<__int64>(ms) * convert_ms_to_100ns_units;
  LARGE_INTEGER timeout = {0};
  timeout.QuadPart = -timeout_100ns;
  return timeout;
}

// Local System and admins get admin_access_mask. Authenticated non-admins get
// non_admin_access_mask access.
void GetEveryoneDaclSecurityDescriptor(CSecurityDesc* sd,
                                       ACCESS_MASK admin_access_mask,
                                       ACCESS_MASK non_admin_access_mask) {
  ASSERT1(sd);

  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), admin_access_mask);
  dacl.AddAllowedAce(Sids::Admins(), admin_access_mask);
  dacl.AddAllowedAce(Sids::Interactive(), non_admin_access_mask);

  sd->SetDacl(dacl);
  sd->MakeAbsolute();
}

bool GetCurrentUserDefaultSecurityAttributes(CSecurityAttributes* sec_attr) {
  ASSERT1(sec_attr);

  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY)) {
    return false;
  }

  CSecurityDesc security_desc;
  CSid sid_owner;
  if (!token.GetOwner(&sid_owner)) {
    return false;
  }

  security_desc.SetOwner(sid_owner);
  CSid sid_group;
  if (!token.GetPrimaryGroup(&sid_group)) {
    return false;
  }

  security_desc.SetGroup(sid_group);

  CDacl dacl;
  if (!token.GetDefaultDacl(&dacl)) {
    return false;
  }

  CSid sid_user;
  if (!token.GetUser(&sid_user)) {
    return false;
  }
  if (!dacl.AddAllowedAce(sid_user, GENERIC_ALL)) {
    return false;
  }

  security_desc.SetDacl(dacl);
  sec_attr->Set(security_desc);

#ifdef DEBUG
  CString sddl;
  security_desc.ToString(&sddl);
  UTIL_LOG(L3, (_T("[GetCurrentUserDefaultSecurityAttributes][%s]"), sddl));
#endif

  return true;
}

void GetAdminDaclSecurityDescriptor(CSecurityDesc* sd, ACCESS_MASK accessmask) {
  ASSERT1(sd);

  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), accessmask);
  dacl.AddAllowedAce(Sids::Admins(), accessmask);

  sd->SetOwner(Sids::Admins());
  sd->SetGroup(Sids::Admins());
  sd->SetDacl(dacl);
  sd->MakeAbsolute();
}

void GetAdminDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                    ACCESS_MASK accessmask) {
  ASSERT1(sec_attr);
  CSecurityDesc sd;
  GetAdminDaclSecurityDescriptor(&sd, accessmask);
  sec_attr->Set(sd);
}

HRESULT InitializeClientSecurity() {
  return ::CoInitializeSecurity(
      NULL,
      -1,
      NULL,   // Let COM choose what authentication services to register.
      NULL,
      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  // Data integrity and encryption.
      RPC_C_IMP_LEVEL_IMPERSONATE,    // Allow server to impersonate.
      NULL,
      EOAC_DYNAMIC_CLOAKING,
      NULL);
}

HRESULT InitializeServerSecurity(bool allow_calls_from_medium) {
  CSecurityDesc sd;
  DWORD eole_auth_capabilities = EOAC_DYNAMIC_CLOAKING;
  if (allow_calls_from_medium) {
    GetEveryoneDaclSecurityDescriptor(&sd,
                                      COM_RIGHTS_EXECUTE,
                                      COM_RIGHTS_EXECUTE);
    sd.SetOwner(Sids::Admins());
    sd.SetGroup(Sids::Admins());
  } else if (user_info::IsRunningAsSystem()) {
    GetAdminDaclSecurityDescriptor(&sd, COM_RIGHTS_EXECUTE);
  }

  HRESULT hr = ::CoInitializeSecurity(
      const_cast<SECURITY_DESCRIPTOR*>(sd.GetPSECURITY_DESCRIPTOR()),
      -1,
      NULL,
      NULL,
      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
      RPC_C_IMP_LEVEL_IDENTIFY,
      NULL,
      eole_auth_capabilities,
      NULL);
  ASSERT(SUCCEEDED(hr), (_T("[InitializeServerSecurity failed][0x%x]"), hr));
  return hr;
}

// The IGlobalOptions interface is supported from Vista onwards. Callers
// should probably ignore the HRESULT returned, since it is not a critical error
// if turning off exception handling fails.
HRESULT DisableCOMExceptionHandling() {
  CComPtr<IGlobalOptions> options;
  HRESULT hr = options.CoCreateInstance(CLSID_GlobalOptions);
  if (SUCCEEDED(hr)) {
    hr = options->Set(COMGLB_EXCEPTION_HANDLING, COMGLB_EXCEPTION_DONOT_HANDLE);
  }

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[DisableCOMExceptionHandling failed][0x%x]"), hr));
  }

  return hr;
}

// This function is not thread-safe.
bool IsPrivateNamespaceAvailable(bool is_machine) {
  static bool is_initialized = false;
  static bool is_available = false;

  if (!is_machine) {
    // TODO(Omaha): From a security viewpoint, private namespaces do not add
    // much value for the User Omaha. But from a uniformity perspective, makes
    // sense to use for both.
    return false;
  }

  if (is_initialized) {
    return is_available;
  }

  if (!SystemInfo::IsRunningOnVistaOrLater()) {
    is_available = false;
    is_initialized = true;
    return false;
  }

  is_available = EnsurePrivateNamespaceAvailable();
  is_initialized = true;
  return is_available;
}


void GetNamedObjectAttributes(const TCHAR* base_name,
                              bool is_machine,
                              NamedObjectAttributes* attr) {
  ASSERT1(base_name);
  ASSERT1(attr);

  // TODO(Omaha): Enable this code after we have a better understanding of
  // Private Object Namespaces.
#if 0
  if (IsPrivateNamespaceAvailable(is_machine)) {
    attr->name = kGoopdatePrivateNamespacePrefix;
  } else {
    ASSERT1(!SystemInfo::IsRunningOnVistaOrLater());
  }
#endif

  attr->name = omaha::kGlobalPrefix;

  if (!is_machine) {
    CString user_sid;
    VERIFY_SUCCEEDED(omaha::user_info::GetProcessUser(NULL, NULL, &user_sid));
    attr->name += user_sid;
    VERIFY1(GetCurrentUserDefaultSecurityAttributes(&attr->sa));
  } else {
    // Grant access to administrators and system.
    GetAdminDaclSecurityAttributes(&attr->sa, GENERIC_ALL);
  }

  attr->name += base_name;
  UTIL_LOG(L1, (_T("[GetNamedObjectAttributes][named_object=%s]"), attr->name));
}

// For now, required_ace_flags is only supported for SE_REGISTRY_KEY objects.
// INHERITED_ACE may be added to the read ACE flags, so it is excluded from
// the comparison with required_ace_flags.
HRESULT AddAllowedAce(const TCHAR* object_name,
                      SE_OBJECT_TYPE object_type,
                      const CSid& sid,
                      ACCESS_MASK required_permissions,
                      uint8 required_ace_flags) {
  ASSERT1(SE_REGISTRY_KEY == object_type || !required_ace_flags);
  ASSERT1(0 == (required_ace_flags & INHERITED_ACE));

  CDacl dacl;
  if (!AtlGetDacl(object_name, object_type, &dacl)) {
    return HRESULTFromLastError();
  }

  int ace_count = dacl.GetAceCount();
  for (int i = 0; i < ace_count; ++i) {
    CSid sid_entry;
    ACCESS_MASK existing_permissions = 0;
    BYTE existing_ace_flags = 0;
    dacl.GetAclEntry(i,
                     &sid_entry,
                     &existing_permissions,
                     NULL,
                     &existing_ace_flags);
    if (sid_entry == sid &&
        required_permissions == (existing_permissions & required_permissions) &&
        required_ace_flags == (existing_ace_flags & ~INHERITED_ACE)) {
      return S_OK;
    }
  }

  if (!dacl.AddAllowedAce(sid, required_permissions, required_ace_flags) ||
      !AtlSetDacl(object_name, object_type, dacl)) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

HRESULT DeleteDirectoryContents(const TCHAR* dir_name) {
  ASSERT1(dir_name);

  WIN32_FIND_DATA find_data = {0};
  CPath find_file_pattern(dir_name);
  VERIFY1(find_file_pattern.Append(_T("*.*")));

  scoped_hfind hfind(::FindFirstFile(find_file_pattern, &find_data));
  if (!hfind) {
    const HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("::FindFirstFile failed][%s][%#x]"), dir_name, hr));
    return hr;
  }

  HRESULT hr_delete = S_OK;
  do {
    CPath file_or_directory(dir_name);
    VERIFY1(file_or_directory.Append(find_data.cFileName));

    if (!_tcscmp(find_data.cFileName, _T("..")) ||
        !_tcscmp(find_data.cFileName, _T("."))) {
      continue;
    }

    // This code continues on if the following delete fails. It is a best-effort
    // delete-and-continue.
    const HRESULT hr = DeleteBeforeOrAfterReboot(file_or_directory);
    if (FAILED(hr)) {
      hr_delete = hr;
    }
  } while (::FindNextFile(get(hfind), &find_data));

  const DWORD err = ::GetLastError();
  if (err != ERROR_NO_MORE_FILES) {
    UTIL_LOG(LE, (_T("[::FindNextFile() failed][%s][%d]"), dir_name, err));
    return HRESULT_FROM_WIN32(err);
  }

  return hr_delete;
}

// Delete directory files. If failed, try to schedule deletion at next reboot
HRESULT DeleteDirectoryFiles(const TCHAR* dir_name) {
  ASSERT1(dir_name);
  return DeleteWildcardFiles(dir_name, _T("*"));
}

// Delete a set of wildcards within dir_name.
// If unable to delete immediately, try to schedule deletion at next reboot
HRESULT DeleteWildcardFiles(const TCHAR* dir_name, const TCHAR* wildcard_name) {
  ASSERT1(dir_name);
  ASSERT1(wildcard_name);

  HRESULT hr = S_OK;

  WIN32_FIND_DATA find_data;
  SetZero(find_data);

  CString find_file(dir_name);
  find_file += _T('\\');
  find_file += wildcard_name;

  scoped_hfind hfind(::FindFirstFile(find_file, &find_data));
  if (!hfind) {
    if (::GetLastError() == ERROR_NO_MORE_FILES) {
      return S_OK;
    } else {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR, (_T("[DeleteWildcardFiles]")
                             _T("[failed to get first file][0x%08x]"), hr));
      return hr;
    }
  }

  do {
    if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      CString specific_file_name(dir_name);
      specific_file_name += _T('\\');
      specific_file_name += find_data.cFileName;
      if (!::DeleteFile(specific_file_name)) {
        if (!SUCCEEDED(hr = File::DeleteAfterReboot(specific_file_name))) {
          UTIL_LOG(LEVEL_ERROR, (_T("[DeleteWildcardFiles]")
                                 _T("[failed to delete after reboot]")
                                 _T("[%s][0x%08x]"), specific_file_name, hr));
        }
      }
    }
  } while (::FindNextFile(get(hfind), &find_data));

  if (::GetLastError() != ERROR_NO_MORE_FILES) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[DeleteWildcardFiles]")
                           _T("[failed to get next file][0x%08x]"), hr));
  }

  return hr;
}

// Delete directory and files within. If failed, try to schedule deletion at
// next reboot
// TODO(Omaha) - the code to delete the directory is complicated,
// especially the way the result code is built from hr and hr1. I wonder if we
// could simplify this by reimplementing it on top of SHFileOperation and
// also save a few tens of bytes in the process.
HRESULT DeleteDirectory(const TCHAR* dir_name) {
  ASSERT1(dir_name);

  if (!SafeDirectoryNameForDeletion(dir_name)) {
    return E_FAIL;
  }

  // Make sure the directory exists (it is ok if it doesn't)
  DWORD dir_attributes = ::GetFileAttributes(dir_name);
  if (dir_attributes == INVALID_FILE_ATTRIBUTES) {
    if (::GetLastError() == ERROR_FILE_NOT_FOUND)
      return S_OK;  // Ok if directory is missing
    else
      return HRESULTFromLastError();
  }
  // Confirm it is a non-redirected directory.
  if (!(dir_attributes & FILE_ATTRIBUTE_DIRECTORY) ||
      (dir_attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
    return E_FAIL;
  }

  // Try to delete all files at best effort
  // Return the first HRESULT error encountered

  // First delete all the normal files
  HRESULT hr = DeleteDirectoryFiles(dir_name);

  // Recursively delete any subdirectories

  WIN32_FIND_DATA find_data = {0};

  CString find_file(dir_name);
  find_file += _T("\\*");

  // Note that the follows are enclosed in a block because we need to close the
  // find handle before deleting the directorty itself
  {
    scoped_hfind hfind(::FindFirstFile(find_file, &find_data));
    if (!hfind) {
      if (::GetLastError() == ERROR_NO_MORE_FILES) {
        return hr;
      } else {
        HRESULT hr1 = HRESULTFromLastError();
        UTIL_LOG(LEVEL_ERROR, (_T("[DeleteDirectory]")
                               _T("[failed to get first file][0x%08x]"), hr1));
        return SUCCEEDED(hr) ? hr1 : hr;
      }
    }

    do {
      if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (String_StrNCmp(find_data.cFileName, _T("."), 2, false) == 0 ||
            String_StrNCmp(find_data.cFileName, _T(".."), 3, false) == 0) {
          continue;
        }

        CString sub_dir(dir_name);
        sub_dir += _T("\\");
        sub_dir += find_data.cFileName;
        HRESULT hr1 = DeleteDirectory(sub_dir);
        if (SUCCEEDED(hr) && FAILED(hr1)) {
          hr = hr1;
        }
      }
    } while (::FindNextFile(get(hfind), &find_data));
  }

  // Delete the empty directory itself
  if (!::RemoveDirectory(dir_name)) {
    HRESULT hr1 = E_FAIL;
    if (FAILED(hr1 = File::DeleteAfterReboot(dir_name))) {
      UTIL_LOG(LE, (_T("[DeleteDirectory][failed to delete after reboot]")
                    _T("[%s][0x%08x]"), dir_name, hr1));
    }

    if (SUCCEEDED(hr) && FAILED(hr1)) {
      hr = hr1;
    }
  }

  return hr;
}

// Returns true if this directory name is 'safe' for deletion (doesn't contain
// "..", doesn't specify a drive root)
bool SafeDirectoryNameForDeletion(const TCHAR* dir_name) {
  ASSERT1(dir_name);

  // empty name isn't allowed
  if (!(dir_name && *dir_name)) {
    return false;
  }

  // require a character other than \/:. after the last :
  // disallow anything with ".."
  bool ok = false;
  for (const TCHAR* s = dir_name; *s; ++s) {
    if (*s != _T('\\') && *s != _T('/') && *s != _T(':') && *s != _T('.')) {
      ok = true;
    }
    if (*s == _T('.') && s > dir_name && *(s-1) == _T('.')) {
      return false;
    }
    if (*s == _T(':')) {
      ok = false;
    }
  }
  return ok;
}

// Utility function that deletes either a file or directory,
// before or after reboot
HRESULT DeleteBeforeOrAfterReboot(const TCHAR* targetname) {
  if (!File::Exists(targetname)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  HRESULT hr = E_FAIL;
  bool is_reparse_point = true;
  if (File::IsDirectory(targetname)) {
    // DeleteDirectory will schedule deletion at next reboot if it cannot delete
    // immediately.
    hr = DeleteDirectory(targetname);
  } else if (SUCCEEDED(File::IsReparsePoint(targetname, &is_reparse_point)) &&
             !is_reparse_point) {
    hr = File::Remove(targetname);
    // If failed, schedule deletion at next reboot
    if (FAILED(hr)) {
      UTIL_LOG(L1, (_T("[DeleteBeforeOrAfterReboot]")
                    _T("[trying to delete after reboot][%s]"), targetname));
      hr = File::DeleteAfterReboot(targetname);
    }
  }

  if (FAILED(hr)) {
    UTIL_LOG(L1, (_T("[DeleteBeforeOrAfterReboot]")
                  _T("[failed to delete][%s][0x%08x]"), targetname, hr));
  }

  return hr;
}


// Internal implementation of the safe version of getting size of all files in
// a directory. It is able to abort the counting if one of the maximum criteria
// is reached.
HRESULT InternalSafeGetDirectorySize(const TCHAR* dir_name,
                                     uint64* size,
                                     HANDLE shutdown_event,
                                     uint64 max_size,
                                     int curr_file_count,
                                     int max_file_count,
                                     int curr_depth,
                                     int max_depth,
                                     DWORD end_time_ms) {
  ASSERT1(dir_name && *dir_name);
  ASSERT1(size);

  CString dir_find_name = String_MakeEndWith(dir_name, _T("\\"), false);
  dir_find_name += _T("*");
  WIN32_FIND_DATA find_data = {0};
  scoped_hfind hfind(::FindFirstFile(dir_find_name, &find_data));
  if (!hfind) {
    return ::GetLastError() == ERROR_NO_MORE_FILES ? S_OK :
                                                     HRESULTFromLastError();
  }

  do {
    // Bail out if shutting down
    if (shutdown_event && IsHandleSignaled(shutdown_event)) {
      return E_ABORT;
    }

    // Bail out if reaching maximum running time
    if (end_time_ms && ::GetTickCount() >= end_time_ms) {
      UTIL_LOG(L6, (_T("[InternalSafeGetDirectorySize]")
                    _T("[reaching max running time][%s][%u]"),
                    dir_name, end_time_ms));
      return E_ABORT;
    }

    // Skip reparse point since it might be a hard link which could cause an
    // infinite recursive directory loop.
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      continue;
    }

    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      // Skip . and ..
      if (String_StrNCmp(find_data.cFileName, _T("."), 2, false) == 0 ||
          String_StrNCmp(find_data.cFileName, _T(".."), 3, false) == 0) {
        continue;
      }

      // Bail out if reaching maximum depth
      if (max_depth && curr_depth + 1 >= max_depth) {
        UTIL_LOG(L6, (_T("[InternalSafeGetDirectorySize]")
                      _T("[reaching max depth][%s][%u]"), dir_name, max_depth));
        return E_ABORT;
      }

      // Walk over sub-directory
      CString sub_dir_name = String_MakeEndWith(dir_name, _T("\\"), false);
      sub_dir_name += find_data.cFileName;
      RET_IF_FAILED(InternalSafeGetDirectorySize(sub_dir_name,
                                                 size,
                                                 shutdown_event,
                                                 max_size,
                                                 curr_file_count,
                                                 max_file_count,
                                                 curr_depth + 1,
                                                 max_depth,
                                                 end_time_ms));
    } else {
      // Bail out if reaching maximum number of files
      ++curr_file_count;
      if (max_file_count && curr_file_count >= max_file_count) {
        UTIL_LOG(L6, (_T("[InternalSafeGetDirectorySize]")
                      _T("[reaching max file count][%s][%u]"),
                      dir_name, max_file_count));
        return E_ABORT;
      }

      // Count the file size
      uint64 file_size =
          ((static_cast<uint64>((find_data.nFileSizeHigh)) << 32)) +
          static_cast<uint64>(find_data.nFileSizeLow);
      *size += file_size;

      // Bail out if reaching maximum size
      if (max_size && *size >= max_size) {
        UTIL_LOG(L6, (_T("[InternalSafeGetDirectorySize]")
                      _T("[reaching max size][%s][%u]"), dir_name, max_size));
        return E_ABORT;
      }
    }
  } while (::FindNextFile(get(hfind), &find_data));

  return ::GetLastError() == ERROR_NO_MORE_FILES ? S_OK :
                                                   HRESULTFromLastError();
}

// The safe version of getting size of all files in a directory
// It is able to abort the counting if one of the maximum criteria is reached
HRESULT SafeGetDirectorySize(const TCHAR* dir_name,
                             uint64* size,
                             HANDLE shutdown_event,
                             uint64 max_size,
                             int max_file_count,
                             int max_depth,
                             int max_running_time_ms) {
  ASSERT1(dir_name && *dir_name);
  ASSERT1(size);

  *size = 0;

  DWORD end_time = 0;
  if (max_running_time_ms > 0) {
    end_time = ::GetTickCount() + max_running_time_ms;
  }
  return InternalSafeGetDirectorySize(dir_name,
                                      size,
                                      shutdown_event,
                                      max_size,
                                      0,
                                      max_file_count,
                                      0,
                                      max_depth,
                                      end_time);
}

// Get size of all files in a directory
HRESULT GetDirectorySize(const TCHAR* dir_name, uint64* size) {
  ASSERT1(dir_name && *dir_name);
  ASSERT1(size);
  return SafeGetDirectorySize(dir_name, size, NULL, 0, 0, 0, 0);
}

// Handles the logic to determine the handle that was signaled
// as a result of calling *WaitForMultipleObjects.
HRESULT GetSignaledObjectPosition(uint32 cnt, DWORD res, uint32* pos) {
  ASSERT1(pos);

  if (res == WAIT_FAILED) {
    return S_FALSE;
  }

#pragma warning(disable : 4296)
  // C4296: '>=' : expression is always true
  if ((res >= WAIT_OBJECT_0) && (res < WAIT_OBJECT_0 + cnt)) {
    *pos = res - WAIT_OBJECT_0;
    return S_OK;
  }
#pragma warning(default : 4296)

  if ((res >= WAIT_ABANDONED_0) && (res < WAIT_ABANDONED_0 + cnt)) {
    *pos = res - WAIT_ABANDONED_0;
    return S_OK;
  }
  return E_INVALIDARG;
}

// Supports all of the other WaitWithMessage* functions.
//
// Returns:
//    S_OK when the message loop should continue.
//    S_FALSE when it receives something that indicates the
//      loop should quit.
//    E_* only when GetMessage failed
//
// This function is not exposed outside of this file.  Only
// friendly wrappers of it are.
HRESULT WaitWithMessageLoopAnyInternal(
    const HANDLE* phandles,
    uint32 cnt,
    uint32* pos,
    MessageHandlerInternalInterface* message_handler) {
  ASSERT1(pos && message_handler);
  // cnt and phandles are either both zero or both not zero.
  ASSERT1(!cnt == !phandles);
  ASSERT1(cnt <= MAXIMUM_WAIT_OBJECTS);

  // Loop until an error happens or the wait is satisfied by a signaled
  // object or an abandoned mutex.
  for (;;) {
    MSG msg = {0};

    // Process the messages in the input queue.
    while (::PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE)) {
      BOOL ret = false;
      if ((ret = ::GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (ret == -1) {
          HRESULT hr = HRESULTFromLastError();
          UTIL_LOG(LE, (_T("[WaitWithMessageLoopAnyInternal]")
                        _T("[GetMessage failed][0x%08x]"), hr));
          return hr;
        }
        message_handler->Process(&msg, &phandles, &cnt);
      } else {
        // We need to re-post the quit message we retrieved so that it could
        // propagate to the outer layer. Otherwise, the program will seem to
        // "get stuck" in its shutdown code.
        ::PostQuitMessage(static_cast<int>(msg.wParam));
        return S_FALSE;
      }

      // WaitForMultipleObjects fails if cnt == 0.
      if (cnt) {
        // Briefly check the state of the handle array to see if something
        // has signaled as we processed a message.
        ASSERT1(phandles);
        DWORD res = ::WaitForMultipleObjects(cnt, phandles, false, 0);
        ASSERT1(res != WAIT_FAILED);
        HRESULT hr = GetSignaledObjectPosition(cnt, res, pos);
        if (SUCCEEDED(hr)) {
          return hr;
        }
      }
    }

    // The wait with message. It is satisfied by either the objects getting
    // signaled or when messages enter the message queue.
    // TODO(omaha): implementing timeout is a little bit tricky since we
    // want the timeout on the handles only and the native API does not
    // have this semantic.
    //
    // TODO(omaha): use a waitable timer to implement the timeout.
    //
    // When cnt is zero then the execution flow waits here until messages
    // arrive in the input queue. Unlike WaitForMultipleObjects,
    // MsgWaitForMultipleObjects does not error out when cnt == 0.
    const DWORD timeout = INFINITE;
    DWORD res(::MsgWaitForMultipleObjects(cnt, phandles, false, timeout,
                                          QS_ALLINPUT));
    ASSERT((res != WAIT_FAILED),
           (_T("[MsgWaitForMultipleObjects returned WAIT_FAILED][%u]"),
            ::GetLastError()));

    ASSERT1(res != WAIT_TIMEOUT);

    HRESULT hr = GetSignaledObjectPosition(cnt, res, pos);
    if (SUCCEEDED(hr)) {
      return hr;
    }
  }
}

// The simplest implementation of a message processor
void BasicMessageHandler::Process(MSG* msg) {
  ASSERT1(msg);
  ::TranslateMessage(msg);
  ::DispatchMessage(msg);
}

class BasicMessageHandlerInternal : public BasicMessageHandler,
                                    public MessageHandlerInternalInterface {
 public:
  BasicMessageHandlerInternal() {}
  virtual void Process(MSG* msg, const HANDLE**, uint32*) {
    BasicMessageHandler::Process(msg);
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(BasicMessageHandlerInternal);
};


bool WaitWithMessageLoopAny(const std::vector<HANDLE>& handles, uint32* pos) {
  BasicMessageHandlerInternal msg_handler;
  ASSERT1(handles.size() <= std::numeric_limits<uint32_t>::max());
  return WaitWithMessageLoopAnyInternal(&handles.front(),
                                        static_cast<uint32>(handles.size()),
                                        pos,
                                        &msg_handler) != S_FALSE;
}

bool WaitWithMessageLoopAll(const std::vector<HANDLE>& handles) {
  // make a copy of the vector, as objects must be removed from the
  // wait array as they get signaled.
  std::vector<HANDLE> h(handles);

  // The function is mainly implemented in terms of WaitWithMessageLoopAny

  // loop until all objects are signaled.
  while (!h.empty()) {
    uint32 pos(static_cast<uint32>(-1));
    if (!WaitWithMessageLoopAny(h, &pos)) return false;
    ASSERT1(pos < h.size());
    h.erase(h.begin() + pos);   // remove the signaled object and loop
  }

  return true;
}

bool WaitWithMessageLoop(HANDLE h) {
  BasicMessageHandlerInternal msg_handler;
  uint32 pos(static_cast<uint32>(-1));
  bool res =
    WaitWithMessageLoopAnyInternal(&h, 1, &pos, &msg_handler) != S_FALSE;
  if (res) {
    // It's the first and the only handle that it is signaled.
    ASSERT1(pos == 0);
  }
  return res;
}

// Wait with message loop for a certain period of time
bool WaitWithMessageLoopTimed(DWORD ms) {
  scoped_timer timer(::CreateWaitableTimer(NULL,
                                           true,   // manual reset
                                           NULL));
  ASSERT1(get(timer));
  LARGE_INTEGER timeout = MSto100NSRelative(ms);
  BOOL timer_ok = ::SetWaitableTimer(get(timer),
                                     &timeout,
                                     0,
                                     NULL,
                                     NULL,
                                     false);
  ASSERT1(timer_ok);
  return WaitWithMessageLoop(get(timer));
}

MessageLoopWithWait::MessageLoopWithWait() : message_handler_(NULL) {
}

void MessageLoopWithWait::set_message_handler(
    MessageHandlerInterface* message_handler) {
  message_handler_ = message_handler;
}

// The message loop and handle callback routine.
HRESULT MessageLoopWithWait::Process() {
  while (true) {
    ASSERT1(callback_handles_.size() == callbacks_.size());
    ASSERT1(callback_handles_.size() <= std::numeric_limits<uint32_t>::max());

    // The implementation allows for an empty array of handles. Taking the
    // address of elements in an empty container is not allowed so we must
    // deal with this case here.
    uint32 pos(0);
    HRESULT hr = WaitWithMessageLoopAnyInternal(
        callback_handles_.empty() ? NULL : &callback_handles_.front(),
        static_cast<uint32>(callback_handles_.size()),
        &pos,
        this);

    // In addition to E_*, S_FALSE should cause a return to happen here.
    if (hr != S_OK) {
      return hr;
    }

    ASSERT1(pos < callback_handles_.size());
    ASSERT1(callback_handles_.size() == callbacks_.size());

    HANDLE signaled_handle = callback_handles_[pos];
    WaitCallbackInterface* callback_interface = callbacks_[pos];
    RemoveHandleAt(pos);

    if (!callback_interface->HandleSignaled(signaled_handle)) {
      return S_OK;
    }
  }
}

// Handles one messgae and adjust the handles and cnt as appropriate after
// handling the message.
void MessageLoopWithWait::Process(MSG* msg, const HANDLE** handles,
                                  uint32* cnt) {
  ASSERT1(msg && handles && cnt);

  if (message_handler_) {
    message_handler_->Process(msg);
  }

  // Set the handles and count again because they may have changed
  // while processing the message.
  ASSERT1(callback_handles_.size() <= std::numeric_limits<uint32_t>::max());

  *handles = callback_handles_.empty() ? NULL : &callback_handles_.front();
  *cnt = static_cast<uint32>(callback_handles_.size());
}
// Starts waiting on the given handle
bool MessageLoopWithWait::RegisterWaitForSingleObject(
    HANDLE handle, WaitCallbackInterface* callback) {
  ASSERT1(callback_handles_.size() == callbacks_.size());
  ASSERT1(callback != NULL);

  if (callback_handles_.size() >= MAXIMUM_WAIT_OBJECTS - 1) {
    return false;
  }

  // In case the user is registering a handle, that they previous added
  // remove the previous one before adding it back into the array.
  UnregisterWait(handle);
  callback_handles_.push_back(handle);
  callbacks_.push_back(callback);

  ASSERT1(callback_handles_.size() == callbacks_.size());
  return true;
}

// Finds the given handle and stops waiting on it
bool MessageLoopWithWait::UnregisterWait(HANDLE handle) {
  ASSERT1(callback_handles_.size() == callbacks_.size());

  for (uint32 index = 0; index < callback_handles_.size() ; index++) {
    if (callback_handles_[index] == handle) {
      RemoveHandleAt(index);
      return true;
    }
  }
  return false;
}

// Removes the wait handle at the given position
void MessageLoopWithWait::RemoveHandleAt(uint32 pos) {
  ASSERT1(callback_handles_.size() == callbacks_.size());
  ASSERT1(pos < callback_handles_.size());

  callback_handles_.erase(callback_handles_.begin() + pos);
  callbacks_.erase(callbacks_.begin() + pos);

  ASSERT1(callback_handles_.size() == callbacks_.size());
}

HRESULT CallEntryPoint0(const TCHAR* dll_path,
                        const char* function_name,
                        HRESULT* result) {
  ASSERT1(dll_path);
  ASSERT1(::lstrlen(dll_path) > 0);
  ASSERT1(function_name);
  ASSERT1(::strlen(function_name) > 0);
  ASSERT1(result);

  scoped_library dll(::LoadLibrary(dll_path));
  if (!dll) {
    return HRESULTFromLastError();
  }

  HRESULT (*proc)() = reinterpret_cast<HRESULT (*)()>(
      ::GetProcAddress(get(dll), function_name));
  if (!proc) {
    return HRESULT_FROM_WIN32(ERROR_INVALID_FUNCTION);
  }

  *result = (proc)();
  return S_OK;
}

// Register a DLL
HRESULT RegisterDll(const TCHAR* dll_path) {
  HRESULT hr = S_OK;
  HRESULT hr_call = CallEntryPoint0(dll_path, "DllRegisterServer", &hr);
  if (SUCCEEDED(hr_call)) {
    return hr;
  }
  return hr_call;
}

// Unregister a DLL
HRESULT UnregisterDll(const TCHAR* dll_path) {
  HRESULT hr = S_OK;
  HRESULT hr_call = CallEntryPoint0(dll_path, "DllUnregisterServer", &hr);
  if (SUCCEEDED(hr_call)) {
    return hr;
  }
  return hr_call;
}

// Register/unregister an EXE
HRESULT RegisterOrUnregisterExe(const TCHAR* exe_path, const TCHAR* cmd_line) {
  ASSERT1(exe_path);
  ASSERT1(cmd_line);

  // cmd_line parameter really contains the arguments to be passed
  // on the process creation command line.
  PROCESS_INFORMATION pi = {0};
  HRESULT hr = System::StartProcessWithArgsAndInfo(exe_path, cmd_line, &pi);
  if (FAILED(hr)) {
    UTIL_LOG(LEVEL_WARNING, (_T("[RegisterOrUnregisterExe]")
                             _T("[failed to start process]")
                             _T("[%s][%s][0x%08x]"), exe_path, cmd_line, hr));
    return hr;
  }
  // Take ownership of the handles for clean up.
  scoped_thread thread(pi.hThread);
  scoped_process process(pi.hProcess);

  // ATL COM servers return an HRESULT on exit. There is a case in which they
  // return -1 which seems like a bug in ATL. It appears there is no
  // documented convention on what a local server would return for errors.
  // There is a possibility that a server would return Windows errors.

  // Wait on the process to exit and return the exit code of the process.
  DWORD result(::WaitForSingleObject(get(process), INFINITE));
  DWORD exit_code(0);
  if (result == WAIT_OBJECT_0 &&
      ::GetExitCodeProcess(get(process), &exit_code)) {
    return static_cast<HRESULT>(exit_code);
  } else {
    return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
  }
}

// Register a COM Local Server
HRESULT RegisterServer(const TCHAR* exe_path) {
  return RegisterOrUnregisterExe(exe_path, _T("/RegServer"));
}

// Unregister a COM Local Server
HRESULT UnregisterServer(const TCHAR* exe_path) {
  return RegisterOrUnregisterExe(exe_path, _T("/UnregServer"));
}

// Register a Service
HRESULT RegisterService(const TCHAR* exe_path) {
  return RegisterOrUnregisterExe(exe_path, _T("/Service"));
}

// Unregister a Service
HRESULT UnregisterService(const TCHAR* exe_path) {
  // Unregistering a service is via UnregServer
  return RegisterOrUnregisterExe(exe_path, _T("/UnregServer"));
}

// Adapted from gds installer/install/work_list.cpp: InstallServiceExecutable
HRESULT RunService(const TCHAR* service_name) {
  scoped_service manager(::OpenSCManager(NULL,  // local machine
                                         NULL,  // ServicesActive database
                                         STANDARD_RIGHTS_READ));
  ASSERT1(get(manager));
  if (!get(manager)) {
    return HRESULTFromLastError();
  }

  scoped_service service(::OpenService(get(manager), service_name,
                                       SERVICE_START));
  ASSERT1(get(service));
  if (!get(service)) {
    return HRESULTFromLastError();
  }

  UTIL_LOG(L2, (_T("start service")));
  if (!::StartService(get(service), 0, NULL)) {
    return HRESULTFromLastError();
  }
  return S_OK;
}


HRESULT ReadEntireFile(const TCHAR* filepath,
                       uint32 max_len,
                       std::vector<byte>* buffer_out) {
  return ReadEntireFileShareMode(filepath, max_len, 0, buffer_out);
}

HRESULT ReadEntireFileShareMode(const TCHAR* filepath,
                                uint32 max_len,
                                DWORD share_mode,
                                std::vector<byte>* buffer_out) {
  ASSERT1(filepath);
  ASSERT1(buffer_out);

  File file;
  HRESULT hr = file.OpenShareMode(filepath, false, false, share_mode);
  if (FAILED(hr)) {
    // File missing.
    return hr;
  }

  ON_SCOPE_EXIT_OBJ(file, &File::Close);

  uint32 file_len = 0;
  hr = file.GetLength(&file_len);
  if (FAILED(hr)) {
    // Should never happen
    return hr;
  }

  if (max_len != 0 && file_len > max_len) {
    // Too large to consider
    return MEM_E_INVALID_SIZE;
  }

  if (file_len == 0) {
    buffer_out->clear();
    return S_OK;
  }

  size_t old_size = buffer_out->size();
  buffer_out->resize(old_size + file_len);

  uint32 bytes_read = 0;
  hr = file.ReadFromStartOfFile(file_len,
                                &(*buffer_out)[old_size],
                                &bytes_read);
  if (FAILED(hr)) {
    // I/O error of some kind
    return hr;
  }

  if (bytes_read != file_len) {
    // Unexpected length. This could happen when reading a file someone else
    // is writing to such as log files.
    ASSERT1(false);
    return E_UNEXPECTED;
  }

  // All's well that ends well
  return S_OK;
}

HRESULT WriteEntireFile(const TCHAR * filepath,
                        const std::vector<byte>& buffer_in) {
  ASSERT1(filepath);

  if (buffer_in.size() > std::numeric_limits<uint32_t>::max()) {
    return E_INVALIDARG;
  }

  // File::WriteAt doesn't implement clear-on-open-for-write semantics,
  // so just delete the file if it exists instead of writing into it.

  if (File::Exists(filepath)) {
    HRESULT hr = File::Remove(filepath);
    if (FAILED(hr)) {
      return hr;
    }
  }

  File file;
  HRESULT hr = file.Open(filepath, true, false);
  if (FAILED(hr)) {
    return hr;
  }

  ON_SCOPE_EXIT_OBJ(file, &File::Close);

  uint32 bytes_written = 0;
  hr = file.WriteAt(0,
                    &buffer_in.front(),
                    static_cast<uint32>(buffer_in.size()),
                    0,
                    &bytes_written);
  if (FAILED(hr)) {
    return hr;
  }
  if (bytes_written != buffer_in.size()) {
    // This shouldn't happen, caller needs to investigate what's up.
    ASSERT1(false);
    return E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT ExpandEnvLikeStrings(const TCHAR* src,
                             const std::map<CString, CString>& keywords,
                             CString* dest) {
  ASSERT1(src);
  ASSERT1(dest);

  const TCHAR kMarker = _T('%');

  dest->Empty();

  // Loop while finding the marker in the string
  HRESULT hr = S_OK;
  int pos = 0;
  int marker_pos1 = -1;
  while ((marker_pos1 = String_FindChar(src, kMarker, pos)) != -1) {
    // Try to find the right marker
    int marker_pos2 = -1;
    const TCHAR* s = src + marker_pos1 + 1;
    for (; *s; ++s) {
      if (*s == kMarker) {
        // TODO(portability): cast is unsafe.
        marker_pos2 = static_cast<int>(s - src);
        break;
      }
      if (!String_IsIdentifierChar(*s)) {
        break;
      }
    }
    if (marker_pos2 == -1) {
      // Unmatched marker found, skip
      dest->Append(src + pos, marker_pos1 - pos + 1);
      pos = marker_pos1 + 1;
      continue;
    }

    // Get the name - without the % markers on each end
    CString name(src + marker_pos1 + 1, marker_pos2 - marker_pos1 - 1);

    bool found = false;
    for (std::map<CString, CString>::const_iterator it(keywords.begin());
         it != keywords.end();
         ++it) {
      if (_tcsicmp(it->first, name) == 0) {
        dest->Append(src + pos, marker_pos1 - pos);
        dest->Append(it->second);
        found = true;
        break;
      }
    }
    if (!found) {
      // No mapping found
      UTIL_LOG(LE, (_T("[ExpandEnvLikeStrings]")
                    _T("[no mapping found for '%s' in '%s']"), name, src));
      dest->Append(src + pos, marker_pos2 - pos + 1);
      hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    pos = marker_pos2 + 1;
  }

  // TODO(portability): cast is unsafe.
  int len = static_cast<int>(_tcslen(src));
  if (pos < len) {
    dest->Append(src + pos, len - pos);
  }

  return hr;
}

bool IsRegistryPath(const TCHAR* path) {
  return String_StartsWith(path, _T("HKLM\\"), false) ||
         String_StartsWith(path, _T("HKCU\\"), false) ||
         String_StartsWith(path, _T("HKCR\\"), false) ||
         String_StartsWith(path, _T("HKEY_LOCAL_MACHINE\\"), false) ||
         String_StartsWith(path, _T("HKEY_CURRENT_USER\\"), false) ||
         String_StartsWith(path, _T("HKEY_CLASSES_ROOT\\"), false);
}

bool IsUrl(const TCHAR* path) {
  // Currently we only check for "http://" and "https://"
  return String_StartsWith(path, kHttpProto, true) ||
         String_StartsWith(path, kHttpsProto, true);
}


CString GuidToString(const GUID& guid) {
  TCHAR guid_str[40] = {0};
  VERIFY1(::StringFromGUID2(guid, guid_str, arraysize(guid_str)));
  String_ToUpper(guid_str);
  return guid_str;
}

HRESULT StringToGuidSafe(const CString& str, GUID* guid) {
  ASSERT1(guid);
  TCHAR* s = const_cast<TCHAR*>(str.GetString());
  return ::IIDFromString(s, guid);
}

// Helper function to convert a variant containing a list of strings
void VariantToStringList(VARIANT var, std::vector<CString>* list) {
  ASSERT1(list);

  list->clear();

  ASSERT1(V_VT(&var) == VT_DISPATCH);
  CComPtr<IDispatch> obj = V_DISPATCH(&var);
  ASSERT1(obj);

  CComVariant var_length;
  VERIFY_SUCCEEDED(obj.GetPropertyByName(_T("length"), &var_length));
  ASSERT1(V_VT(&var_length) == VT_I4);
  int length = V_I4(&var_length);

  for (int i = 0; i < length; ++i) {
    CComVariant value;
    VERIFY_SUCCEEDED(obj.GetPropertyByName(itostr(i), &value));
    if (V_VT(&value) == VT_BSTR) {
      list->push_back(V_BSTR(&value));
    } else {
      ASSERT1(false);
    }
  }
}


HRESULT GetCurrentProcessHandle(HANDLE* handle) {
  ASSERT1(handle);
  scoped_process real_handle;
  HANDLE pseudo_handle = ::GetCurrentProcess();
  bool res = ::DuplicateHandle(
    pseudo_handle,         // this process pseudo-handle
    pseudo_handle,         // handle to duplicate
    pseudo_handle,         // the process receiving the handle
    address(real_handle),  // this process real handle
    0,                     // ignored
    false,                 // don't inherit this handle
    DUPLICATE_SAME_ACCESS) != 0;

  *handle = NULL;
  if (!res) {
    return HRESULTFromLastError();
  }
  *handle = release(real_handle);
  return S_OK;
}

HRESULT DuplicateTokenIntoCurrentProcess(HANDLE source_process,
                                         HANDLE token_to_duplicate,
                                         CAccessToken* duplicated_token) {
  ASSERT1(source_process);
  ASSERT1(token_to_duplicate);
  ASSERT1(duplicated_token);

  scoped_handle alt_token;
  HRESULT hr = DuplicateHandleIntoCurrentProcess(source_process,
                                                 token_to_duplicate,
                                                 address(alt_token));

  if (SUCCEEDED(hr)) {
    duplicated_token->Attach(release(alt_token));
  }

  return hr;
}

HRESULT DuplicateHandleIntoCurrentProcess(HANDLE source_process,
                                          HANDLE to_duplicate,
                                          HANDLE* destination) {
  ASSERT1(source_process);
  ASSERT1(to_duplicate);
  ASSERT1(destination);

  bool res = ::DuplicateHandle(
      source_process,                 // Process whose handle needs duplicating.
      to_duplicate,                   // Handle to duplicate.
      ::GetCurrentProcess(),          // Current process receives the handle.
      destination,                    // Duplicated handle.
      0,                              // Ignored due to DUPLICATE_SAME_ACCESS.
      FALSE,                          // Do not inherit the new handle.
      DUPLICATE_SAME_ACCESS) != 0;    // Same access as to_duplicate.

  if (!res) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[DuplicateHandleIntoCurrentProcess failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

bool TimeHasElapsed(DWORD baseline, DWORD milisecs) {
  DWORD current = ::GetTickCount();
  DWORD wrap_bias = 0;
  if (current < baseline) {
    wrap_bias = static_cast<DWORD>(0xFFFFFFFF);
  }
  return (current - baseline + wrap_bias) >= milisecs ? true : false;
}

// get a time64 value
// NOTE: If the value is greater than the
// max value, then SetValue will be called using the max_value.
HRESULT GetLimitedTimeValue(const TCHAR* full_key_name, const TCHAR* value_name,
                            time64 max_time, time64* value,
                            bool* limited_value) {
  ASSERT1(full_key_name);
  ASSERT1(value);
  STATIC_ASSERT(sizeof(time64) == sizeof(DWORD64));

  if (limited_value) {
    *limited_value = false;
  }
  HRESULT hr = RegKey::GetValue(full_key_name, value_name, value);
  if (SUCCEEDED(hr) && *value > max_time) {
    *value = max_time;

    // Use a different hr for the setting of the value b/c
    // the returned hr should reflect the success/failure of reading the key
    HRESULT set_value_hr = RegKey::SetValue(full_key_name, value_name, *value);
    ASSERT(SUCCEEDED(set_value_hr),
           (_T("GetLimitedTimeValue - failed when setting a value: 0x%08x]"),
            set_value_hr));
    if (SUCCEEDED(set_value_hr) && limited_value) {
      *limited_value = true;
    }
  }
  return hr;
}

// get a time64 value trying reg keys successively if there is a
// failure in getting a value.
HRESULT GetLimitedTimeValues(const TCHAR* full_key_names[],
                             int key_names_length,
                             const TCHAR* value_name,
                             time64 max_time,
                             time64* value,
                             bool* limited_value) {
  ASSERT1(full_key_names);
  ASSERT1(value);
  ASSERT1(key_names_length > 0);

  HRESULT hr = E_FAIL;
  for (int i = 0; i < key_names_length; ++i) {
    hr = GetLimitedTimeValue(full_key_names[i], value_name, max_time, value,
                             limited_value);
    if (SUCCEEDED(hr)) {
      return hr;
    }
  }
  return hr;
}

// Wininet.dll (and especially the version that comes with IE7, with 01/12/07
// timestamp) incorrectly initializes Rasman.dll. As a result, there is a race
// condition that causes double-free on a memory from process heap.
// This causes memory corruption in the heap that may later produce a variety
// of ill effects, most frequently a crash with a callstack that contains
// wininet and rasman, or ntdll!RtlAllocHeap. The root cause is that
// Rasapi32!LoadRasmanDllAndInit is not thread safe and can start very involved
// process of initialization on 2 threads at the same time. It's a bug.
// Solution: in the begining of the program, trigger synchronous load of
// rasman dll. The easy way is to call a public ras api that does synchronous
// initialization, which is what we do here.
void EnsureRasmanLoaded() {
  RASENTRYNAME ras_entry_name = {0};
  DWORD size_bytes = sizeof(ras_entry_name);
  DWORD number_of_entries = 0;
  ras_entry_name.dwSize = size_bytes;
  // we don't really need results of this method,
  // it simply triggers RASAPI32!LoadRasmanDllAndInit() internally.
  RasEnumEntriesWWrap(NULL,
                      NULL,
                      &ras_entry_name,
                      &size_bytes,
                      &number_of_entries);
}

// Appends two reg keys. Handles the situation where there are traling
// back slashes in one and leading back slashes in two.
CString AppendRegKeyPath(const CString& one, const CString& two) {
  CString leftpart(one);
  int length = leftpart.GetLength();
  int i = 0;
  for (i = length - 1; i >= 0; --i) {
    if (leftpart[i] != _T('\\')) {
      break;
    }
  }
  leftpart = leftpart.Left(i+1);

  CString rightpart(two);
  int lengthr = rightpart.GetLength();
  for (i = 0; i < lengthr; ++i) {
    if (rightpart[i] != _T('\\')) {
      break;
    }
  }
  rightpart = rightpart.Right(lengthr - i);

  CString result;
  SafeCStringFormat(&result, _T("%s\\%s"), leftpart, rightpart);
  return result;
}

CString AppendRegKeyPath(const CString& one, const CString& two,
                         const CString& three) {
  CString result = AppendRegKeyPath(one, two);
  result = AppendRegKeyPath(result, three);
  return result;
}


HRESULT GetUserKeysFromHkeyUsers(std::vector<CString>* key_names) {
  ASSERT1(key_names);
  UTIL_LOG(L3, (_T("[GetUserKeysFromHkeyUsers]")));

  TCHAR user_key_name[MAX_PATH] = {0};
  int i = 0;
  while (::RegEnumKey(HKEY_USERS, i++, user_key_name, MAX_PATH) !=
                      ERROR_NO_MORE_ITEMS) {
        byte sid_buffer[SECURITY_MAX_SID_SIZE] = {0};
    PSID sid = reinterpret_cast<PSID>(sid_buffer);
    if (::ConvertStringSidToSid(user_key_name, &sid) != 0) {
      // We could convert the string SID into a real SID. If not
      // we just ignore.
      DWORD size = MAX_PATH;
      DWORD size_domain = MAX_PATH;
      SID_NAME_USE sid_type = SidTypeComputer;
      TCHAR user_name[MAX_PATH] = {0};
      TCHAR domain_name[MAX_PATH] = {0};

      if (::LookupAccountSid(NULL, sid, user_name, &size,
                             domain_name, &size_domain, &sid_type) == 0) {
        HRESULT hr = HRESULTFromLastError();
        UTIL_LOG(LW, (_T("[GetUserKeysFromHkeyUsers LookupAccountSid failed]")
                      _T("[0x%08x]"), hr));
        continue;
      }

      if (sid_type == SidTypeUser) {
        // Change the RunAs keys for the user goopdates to point to the
        // machine install.
        CString user_reg_key_name = AppendRegKeyPath(USERS_KEY, user_key_name);
        key_names->push_back(user_reg_key_name);
      }
    }
  }

  return S_OK;
}

HRESULT IsSystemProcess(bool* is_system_process) {
  CAccessToken current_process_token;
  if (!current_process_token.GetProcessToken(TOKEN_QUERY,
                                             ::GetCurrentProcess())) {
    HRESULT hr = HRESULTFromLastError();
    ASSERT(false, (_T("CAccessToken::GetProcessToken failed: 0x%08x"), hr));
    return hr;
  }
  CSid logon_sid;
  if (!current_process_token.GetUser(&logon_sid)) {
    HRESULT hr = HRESULTFromLastError();
    ASSERT(false, (_T("CAccessToken::GetUser failed: 0x%08x"), hr));
    return hr;
  }
  *is_system_process = logon_sid == Sids::System();
  return S_OK;
}

HRESULT IsUserLoggedOn(bool* is_logged_on) {
  ASSERT1(is_logged_on);
  bool is_local_system(false);
  HRESULT hr = IsSystemProcess(&is_local_system);
  if (SUCCEEDED(hr) && is_local_system) {
    *is_logged_on = true;
    return S_OK;
  }
  return UserRights::UserIsLoggedOnInteractively(is_logged_on);
}

bool ShellExecuteExEnsureParent(LPSHELLEXECUTEINFO shell_exec_info) {
  UTIL_LOG(L3, (_T("[ShellExecuteExEnsureParent]")));

  ASSERT1(shell_exec_info);

  // Prevents elevation of privilege by reverting to the process token before
  // starting the process. Otherwise, a lower privilege token could for instance
  // symlink `C:\` to a different folder (per-user DosDevice) and allow an
  // elevation of privilege attack.
  scoped_revert_to_self revert_to_self;

  bool shell_exec_succeeded(false);
  DWORD last_error(ERROR_SUCCESS);

  {
    // hwnd_parent window is destroyed at the end of the scope when the
    // destructor of scoped_window calls ::DestroyWindow.
    scoped_window hwnd_parent;

    if (!shell_exec_info->hwnd && vista_util::IsVistaOrLater()) {
      reset(hwnd_parent, CreateForegroundParentWindowForUAC());

      if (!hwnd_parent) {
        last_error = ::GetLastError();
        UTIL_LOG(LE, (_T("[CreateForegroundParentWindowForUAC failed]")));
        // Restore last error in case the logging reset it.
        ::SetLastError(last_error);
        return false;
      }

      shell_exec_info->hwnd = get(hwnd_parent);

      // If elevation is required on Vista, call ::SetForegroundWindow(). This
      // will make sure that the elevation prompt, as well as the elevated
      // process window comes up in the foreground. It will also ensure that in
      // the case where the elevation prompt is cancelled, the error dialog
      // shown from this process comes up in the foreground.
      if (shell_exec_info->lpVerb &&
          _tcsicmp(shell_exec_info->lpVerb, _T("runas")) == 0) {
        if (!::SetForegroundWindow(get(hwnd_parent))) {
          UTIL_LOG(LW, (_T("[SetForegroundWindow failed][%d]"),
                        ::GetLastError()));
        }
      }
    }

    shell_exec_succeeded = !!::ShellExecuteEx(shell_exec_info);

    if (shell_exec_succeeded) {
      if (shell_exec_info->hProcess) {
        DWORD pid = Process::GetProcessIdFromHandle(shell_exec_info->hProcess);
        OPT_LOG(L1, (_T("[Started process][%u]"), pid));
        if (!::AllowSetForegroundWindow(pid)) {
          UTIL_LOG(LW, (_T("[AllowSetForegroundWindow failed][%d]"),
                        ::GetLastError()));
        }
      } else {
        OPT_LOG(L1, (_T("[Started process][PID unknown]")));
      }
    } else {
      last_error = ::GetLastError();
      UTIL_LOG(LE, (_T("[ShellExecuteEx failed][%s][%s][0x%08x]"),
                    shell_exec_info->lpFile, shell_exec_info->lpParameters,
                    last_error));
    }
  }

  // The implicit ::DestroyWindow call from the scoped_window could have reset
  // the last error, so restore it.
  ::SetLastError(last_error);

  return shell_exec_succeeded;
}

// Assumes the path in command is properly enclosed if necessary.
HRESULT ConfigureRunAtStartup(const CString& root_key_name,
                              const CString& run_value_name,
                              const CString& command,
                              bool install) {
  UTIL_LOG(L3, (_T("[ConfigureRunAtStartup]")));

  const CString key_path = AppendRegKeyPath(root_key_name, REGSTR_PATH_RUN);
  HRESULT hr(S_OK);

  if (install) {
    hr = RegKey::SetValue(key_path, run_value_name, command);
  } else {
    hr = RegKey::DeleteValue(key_path, run_value_name);
  }

  return hr;
}

HRESULT GetExePathFromCommandLine(const TCHAR* command_line,
                                  CString* exe_path) {
  ASSERT1(exe_path);
  CString command_line_str(command_line);
  command_line_str.Trim(_T(' '));
  if (command_line_str.IsEmpty()) {
    // ::CommandLineToArgvW parses the current process command line for blank
    // strings. We do not want this behavior.
    return E_INVALIDARG;
  }

  int argc = 0;
  wchar_t** argv = ::CommandLineToArgvW(command_line_str, &argc);
  if (argc == 0 || !argv) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[::CommandLineToArgvW failed][0x%08x]"), hr));
    return hr;
  }

  *exe_path = argv[0];
  ::LocalFree(argv);
  exe_path->Trim(_T(' '));
  ASSERT1(!exe_path->IsEmpty());
  return S_OK;
}

// Tries to open the _MSIExecute mutex and tests its state. MSI sets the
// mutex when processing sequence tables. This indicates MSI is busy.
// The function returns S_OK if the mutex is not owned by MSI or the mutex has
// not been created.
HRESULT WaitForMSIExecute(int timeout_ms) {
  const TCHAR* mutex_name = _T("Global\\_MSIExecute");
  scoped_mutex mutex(::OpenMutex(SYNCHRONIZE, false, mutex_name));
  if (!mutex) {
    DWORD error = ::GetLastError();
    return (error == ERROR_FILE_NOT_FOUND) ? S_OK : HRESULT_FROM_WIN32(error);
  }
  UTIL_LOG(L3, (_T("[Wait for _MSIExecute]")));
  switch (::WaitForSingleObject(get(mutex), timeout_ms)) {
    case WAIT_OBJECT_0:
    case WAIT_ABANDONED:
      VERIFY1(::ReleaseMutex(get(mutex)));
      return S_OK;
    case WAIT_TIMEOUT:
      return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    case WAIT_FAILED:
      return HRESULTFromLastError();
    default:
      return E_FAIL;
  }
}

// States are documented at
// http://technet.microsoft.com/en-us/library/cc721913.aspx.
bool IsWindowsInstalling() {
  static const TCHAR kVistaSetupStateKey[] =
      _T("Software\\Microsoft\\Windows\\CurrentVersion\\Setup\\State");
  static const TCHAR kImageStateValueName[] = _T("ImageState");
  static const TCHAR kImageStateUnuseableValue[] =
      _T("IMAGE_STATE_UNDEPLOYABLE");
  static const TCHAR kImageStateGeneralAuditValue[] =
      _T("IMAGE_STATE_GENERALIZE_RESEAL_TO_AUDIT");
  static const TCHAR kImageStateSpecialAuditValue[] =
      _T("IMAGE_STATE_SPECIALIZE_RESEAL_TO_AUDIT");

  static const TCHAR kXPSetupStateKey[] = _T("System\\Setup");
  static const TCHAR kAuditFlagValueName[] = _T("AuditInProgress");

  if (vista_util::IsVistaOrLater()) {
    RegKey vista_setup_key;
    HRESULT hr =
        vista_setup_key.Open(HKEY_LOCAL_MACHINE, kVistaSetupStateKey, KEY_READ);
    if (SUCCEEDED(hr)) {
      CString state;
      hr = vista_setup_key.GetValue(kImageStateValueName, &state);
      if (SUCCEEDED(hr) &&
          !state.IsEmpty() &&
          (0 == state.CompareNoCase(kImageStateUnuseableValue) ||
           0 == state.CompareNoCase(kImageStateGeneralAuditValue) ||
           0 == state.CompareNoCase(kImageStateSpecialAuditValue)))
        return true;  // Vista is still installing.
    }
  } else {
    RegKey xp_setup_key;
    HRESULT hr =
        xp_setup_key.Open(HKEY_LOCAL_MACHINE, kXPSetupStateKey, KEY_READ);
    if (SUCCEEDED(hr)) {
      DWORD audit_flag(0);
      hr = xp_setup_key.GetValue(kAuditFlagValueName, &audit_flag);
      if (SUCCEEDED(hr) && 0 != audit_flag)
        return true;  // XP is still installing.
    }
  }
  return false;
}

HRESULT GetGuid(CString* guid) {
  GUID guid_local = {0};
  HRESULT hr = ::CoCreateGuid(&guid_local);
  if (FAILED(hr)) {
    return hr;
  }
  *guid = GuidToString(guid_local);
  return S_OK;
}

CString GetMessageForSystemErrorCode(DWORD error_code) {
  UTIL_LOG(L3, (_T("[GetMessageForSystemErrorCode][%u]"), error_code));

  TCHAR* system_allocated_buffer = NULL;
  const DWORD kFormatOptions = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                               FORMAT_MESSAGE_FROM_SYSTEM |
                               FORMAT_MESSAGE_IGNORE_INSERTS |
                               FORMAT_MESSAGE_MAX_WIDTH_MASK;
  DWORD tchars_written = ::FormatMessage(
      kFormatOptions,
      NULL,
      error_code,
      0,
      reinterpret_cast<LPWSTR>(&system_allocated_buffer),
      0,
      NULL);

  CString message;
  if (tchars_written > 0) {
    message = system_allocated_buffer;
  } else {
    UTIL_LOG(LW, (_T("[::FormatMessage failed][%u]"), ::GetLastError()));
  }

  VERIFY1(!::LocalFree(system_allocated_buffer));

  return message;
}

CString GetTempFilename(const TCHAR* prefix) {
  ASSERT1(prefix);

  CString temp_dir = app_util::GetTempDir();
  VERIFY1(!temp_dir.IsEmpty());

  return GetTempFilenameAt(temp_dir, prefix);
}

DWORD WaitForAllObjects(size_t count, const HANDLE* handles, DWORD timeout) {
  if (count <= MAXIMUM_WAIT_OBJECTS) {
    return ::WaitForMultipleObjects(static_cast<DWORD>(count),
                                    handles,
                                    TRUE,
                                    timeout);
  }

  UTIL_LOG(L3, (_T("[WaitForAllObjects][%Iu][%u ms]"), count, timeout));

  // Spin in a loop, calling ::WFMO() on blocks of handles at a time. If it
  // returns WAIT_TIMEOUT or WAIT_FAILED, we can immediately exit without
  // having to check any more handles. For successful return values, we find
  // the total time we spent waiting, and subtract that from the timeout on
  // the next call.

  const DWORD start_time = GetTickCount();
  DWORD time_waited = 0;
  bool abandoned = false;
  for (size_t i = 0; i < count; i += MAXIMUM_WAIT_OBJECTS) {
    DWORD num_waits;
    if ((i + MAXIMUM_WAIT_OBJECTS) <= count) {
      num_waits = MAXIMUM_WAIT_OBJECTS;
    } else {
      num_waits = static_cast<DWORD>(count - i);
    }

    DWORD result = ::WaitForMultipleObjects(num_waits, handles + i,
                                            TRUE, timeout - time_waited);

    if (result == WAIT_TIMEOUT || result == WAIT_FAILED) {
      return result;
    }
    if (result >= WAIT_ABANDONED_0 && result < (WAIT_ABANDONED_0 + num_waits)) {
      abandoned = true;
    }

    if (timeout != INFINITE) {
      time_waited = GetTickCount() - start_time;
      if (time_waited > timeout) {
        // If the timeout has passed, clamp it, so that ::WFMO() is called with
        // a timeout of zero. This makes the call non-blocking, but means we
        // can still return WAIT_OBJECT_0 if later blocks are all signaled.
        time_waited = timeout;
      }
    }
  }

  // All handles are either signaled or abandoned.
  return abandoned ? WAIT_ABANDONED_0 : WAIT_OBJECT_0;
}

bool IsEnrolledToDomain() {
  DWORD is_enrolled(false);
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueIsEnrolledToDomain,
                                 &is_enrolled))) {
    return !!is_enrolled;
  }

  return EnrolledToDomainStatus() == ENROLLED;
}

// The following code is cloned/derived from
// https://cs.chromium.org/chromium/src/base/win/win_util.cc.
static volatile LONG g_domain_state = UNKNOWN;

DomainEnrollmentState EnrolledToDomainStatus() {
  if (g_domain_state == UNKNOWN) {
    LPWSTR domain;
    NETSETUP_JOIN_STATUS join_status;
    if (::NetGetJoinInformation(NULL, &domain, &join_status) != NERR_Success) {
      return UNKNOWN;
    }

    ::NetApiBufferFree(domain);
    ::InterlockedCompareExchange(&g_domain_state,
                                 join_status == ::NetSetupDomainName ?
                                     ENROLLED :
                                     (join_status == ::NetSetupUnknownStatus ?
                                      UNKNOWN_ENROLLED : NOT_ENROLLED),
                                 UNKNOWN);
  }

  return static_cast<DomainEnrollmentState>(g_domain_state);
}

enum DeviceRegisteredState {NOT_KNOWN = -1, NOT_REGISTERED, REGISTERED};
static volatile LONG g_registered_state = NOT_KNOWN;

bool IsDeviceRegisteredWithManagement() {
  if (g_registered_state == NOT_KNOWN) {
    BOOL is_registered = FALSE;
    HRESULT hr(IsDeviceRegisteredWithManagementWrap(&is_registered, 0, NULL));
    ::InterlockedCompareExchange(&g_registered_state,
                                 SUCCEEDED(hr) && is_registered ?
                                     REGISTERED : NOT_REGISTERED,
                                 NOT_KNOWN);
  }

  return g_registered_state == REGISTERED;
}

enum AzureADState {AZUREAD_NOT_KNOWN = -1, AZUREAD_NOT_JOINED, AZUREAD_JOINED};
static volatile LONG g_azure_ad_state = AZUREAD_NOT_KNOWN;

bool IsJoinedToAzureAD() {
  if (g_azure_ad_state == AZUREAD_NOT_KNOWN) {
    PDSREG_JOIN_INFO join_info = NULL;

    // |join_info| is non-NULL if the device is joined to Azure AD or the
    // current user added Azure AD work accounts.
    HRESULT hr(NetGetAadJoinInformationWrap(NULL, &join_info));
    ::InterlockedCompareExchange(&g_azure_ad_state,
                                 SUCCEEDED(hr) && join_info ?
                                     AZUREAD_JOINED : AZUREAD_NOT_JOINED,
                                 AZUREAD_NOT_KNOWN);
    if (SUCCEEDED(hr)) {
      NetFreeAadJoinInformationWrap(join_info);
    }
  }

  return g_azure_ad_state == AZUREAD_JOINED;
}

static volatile LONG g_os_version_type = SUITE_LAST;

bool IsEnterpriseManaged() {
  if (g_os_version_type == SUITE_LAST) {
    ::InterlockedCompareExchange(&g_os_version_type,
                                 SystemInfo::GetOSVersionType(),
                                 SUITE_LAST);
  }

  bool is_enterprise_version = (g_os_version_type != SUITE_HOME);

  return IsEnrolledToDomain() ||
         (is_enterprise_version &&
          ((EnrolledToDomainStatus() == UNKNOWN_ENROLLED) ||
           IsDeviceRegisteredWithManagement() ||
           IsJoinedToAzureAD()));
}

HMODULE LoadSystemLibrary(const TCHAR* library_name) {
  ASSERT1(!IsAbsolutePath(library_name));
  const CString system_dir = app_util::GetSystemDir();
  if (system_dir.IsEmpty()) {
    return nullptr;
  }
  return LoadSystemLibraryHelper(ConcatenatePath(system_dir, library_name));
}

}  // namespace omaha

