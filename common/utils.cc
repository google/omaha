// Copyright 2003-2009 Google Inc.
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

#include "omaha/common/utils.h"

#include <ras.h>
#include <regstr.h>
#include <urlmon.h>
#include <wincrypt.h>
#include <ATLComTime.h>
#include <atlpath.h>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/app_util.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/const_config.h"
#include "omaha/common/const_timeouts.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/process.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/shell.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/common/system.h"
#include "omaha/common/system_info.h"
#include "omaha/common/time.h"
#include "omaha/common/user_info.h"
#include "omaha/common/user_rights.h"
#include "omaha/common/vistautil.h"

namespace omaha {

namespace {

// Private object namespaces for Vista processes.
const TCHAR* const kGoopdateBoundaryDescriptor = _T("GoogleUpdate_BD");
const TCHAR* const kGoopdatePrivateNamespace = _T("GoogleUpdate");
const TCHAR* const kGoopdatePrivateNamespacePrefix = _T("GoogleUpdate\\");

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
    DWORD last_error(::GetLastError());
    UTIL_LOG(LE, (_T("::CreateWellKnownSid failed[%d]"), last_error));
    return false;
  }
  if (!AddSIDToBoundaryDescriptorWrap(&boundary_descriptor, sid)) {
    DWORD last_error(::GetLastError());
    UTIL_LOG(LE, (_T("AddSIDToBoundaryDescriptor fail[%d]"), last_error));
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
         (_T("CreatePrivateNamespaceW failed. %d"), ::GetLastError()));

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

    if (kuint16max < quad[i]) {
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
  version_string.Format((_T("%u.%u.%u.%u")),
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
    CORE_LOG(LEVEL_WARNING, (_T("[CoCreateGuid failed][0x%08x]"), hr));
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

// Use of this method is unsafe. Be careful!
// Local System and admins get GENERIC_ALL access. Authenticated non-admins get
// non_admin_access_mask access.
void GetEveryoneDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                       ACCESS_MASK non_admin_access_mask) {
  ASSERT1(sec_attr);

  // Grant access to all users.
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), GENERIC_ALL);
  dacl.AddAllowedAce(Sids::Admins(), GENERIC_ALL);
  dacl.AddAllowedAce(Sids::Interactive(), non_admin_access_mask);

  CSecurityDesc security_descriptor;
  security_descriptor.SetDacl(dacl);
  security_descriptor.MakeAbsolute();

  sec_attr->Set(security_descriptor);
}

void GetAdminDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                    ACCESS_MASK accessmask) {
  ASSERT1(sec_attr);
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), accessmask);
  dacl.AddAllowedAce(Sids::Admins(), accessmask);

  CSecurityDesc security_descriptor;
  security_descriptor.SetOwner(Sids::Admins());
  security_descriptor.SetGroup(Sids::Admins());
  security_descriptor.SetDacl(dacl);
  security_descriptor.MakeAbsolute();

  sec_attr->Set(security_descriptor);
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
#endif

  attr->name = omaha::kGlobalPrefix;

  if (!is_machine) {
    CString user_sid;
    VERIFY1(SUCCEEDED(omaha::user_info::GetCurrentUser(NULL, NULL, &user_sid)));
    attr->name += user_sid;
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

HRESULT CreateDir(const TCHAR* in_dir,
                  LPSECURITY_ATTRIBUTES security_attr) {
  ASSERT1(in_dir);
  CString path;
  if (!PathCanonicalize(CStrBuf(path, MAX_PATH), in_dir)) {
    return E_FAIL;
  }
  // Standardize path on backslash so Find works.
  path.Replace(_T('/'), _T('\\'));
  int next_slash = path.Find(_T('\\'));
  while (true) {
    int len = 0;
    if (next_slash == -1) {
      len = path.GetLength();
    } else {
      len = next_slash;
    }
    CString dir(path.Left(len));
    // The check for File::Exists should not be needed. However in certain
    // cases, i.e. when the program is run from a n/w drive or from the
    // root drive location, the first CreateDirectory fails with an
    // E_ACCESSDENIED instead of a ALREADY_EXISTS. Hence we protect the call
    // with the exists.
    if (!File::Exists(dir)) {
      if (!::CreateDirectory(dir, security_attr)) {
        DWORD error = ::GetLastError();
        if (ERROR_FILE_EXISTS != error && ERROR_ALREADY_EXISTS != error) {
          return HRESULT_FROM_WIN32(error);
        }
      }
    }
    if (next_slash == -1) {
      break;
    }
    next_slash = path.Find(_T('\\'), next_slash + 1);
  }

  return S_OK;
}

HRESULT GetFolderPath(int csidl, CString* path) {
  if (!path) {
    return E_INVALIDARG;
  }

  TCHAR buffer[MAX_PATH] = {0};
  HRESULT hr = ::SHGetFolderPath(NULL, csidl, NULL, SHGFP_TYPE_CURRENT, buffer);
  if (FAILED(hr)) {
    return hr;
  }

  *path = buffer;
  return S_OK;
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
                             _T("[failed to get first file][0x%x]"), hr));
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
                                 _T("[%s][0x%x]"), specific_file_name, hr));
        }
      }
    }
  } while (::FindNextFile(get(hfind), &find_data));

  if (::GetLastError() != ERROR_NO_MORE_FILES) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[DeleteWildcardFiles]")
                           _T("[failed to get next file][0x%x]"), hr));
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
  // Confirm it is a directory
  if (!(dir_attributes & FILE_ATTRIBUTE_DIRECTORY)) {
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
                               _T("[failed to get first file][0x%x]"), hr1));
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
    }
    while (::FindNextFile(get(hfind), &find_data));
  }

  // Delete the empty directory itself
  if (!::RemoveDirectory(dir_name)) {
    HRESULT hr1 = E_FAIL;
    if (FAILED(hr1 = File::DeleteAfterReboot(dir_name))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[DeleteDirectory]")
                             _T("[failed to delete after reboot]")
                             _T("[%s][0x%x]"), dir_name, hr1));
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
  if (File::IsDirectory(targetname)) {
    // DeleteDirectory will schedule deletion at next reboot if it cannot delete
    // immediately.
    hr = DeleteDirectory(targetname);
  } else  {
    hr = File::Remove(targetname);
    // If failed, schedule deletion at next reboot
    if (FAILED(hr)) {
      UTIL_LOG(L1, (_T("[DeleteBeforeOrAfterReboot]")
                    _T("[trying to delete %s after reboot]"), targetname));
      hr = File::DeleteAfterReboot(targetname);
    }
  }

  if (FAILED(hr)) {
    UTIL_LOG(L1, (_T("[DeleteBeforeOrAfterReboot]")
                  _T("[failed to delete %s]"), targetname));
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
          UTIL_LOG(LEVEL_ERROR,
              (_T("[WaitWithMessageLoopAny - GetMessage failed][0x%08x]"), hr));
          return hr;
        }
        message_handler->Process(&msg, &phandles, &cnt);
      } else {
        // We need to re-post the quit message we retrieved so that it could
        // propagate to the outer layer. Otherwise, the program will seem to
        // "get stuck" in its shutdown code.
        ::PostQuitMessage(msg.wParam);
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
  DISALLOW_EVIL_CONSTRUCTORS(BasicMessageHandlerInternal);
};


bool WaitWithMessageLoopAny(const std::vector<HANDLE>& handles, uint32* pos) {
  BasicMessageHandlerInternal msg_handler;
  return WaitWithMessageLoopAnyInternal(&handles.front(), handles.size(), pos,
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

    // The implementation allows for an empty array of handles. Taking the
    // address of elements in an empty container is not allowed so we must
    // deal with this case here.
    size_t pos(0);
    HRESULT hr = WaitWithMessageLoopAnyInternal(
        callback_handles_.empty() ? NULL : &callback_handles_.front(),
        callback_handles_.size(),
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
  *handles = callback_handles_.empty() ? NULL : &callback_handles_.front();
  *cnt = callback_handles_.size();
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
  DWORD result(::WaitForSingleObject(get(process), kRegisterExeTimeoutMs));
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

  int old_size = buffer_out->size();
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
  hr = file.WriteAt(0, &buffer_in.front(), buffer_in.size(), 0, &bytes_written);
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

// Conversions between a byte stream and a std::string
HRESULT BufferToString(const std::vector<byte>& buffer_in, CStringA* str_out) {
  ASSERT1(str_out);
  str_out->Append(reinterpret_cast<const char*>(&buffer_in.front()),
                  buffer_in.size());
  return S_OK;
}

HRESULT StringToBuffer(const CStringA& str_in, std::vector<byte>* buffer_out) {
  ASSERT1(buffer_out);
  buffer_out->assign(str_in.GetString(),
                     str_in.GetString() + str_in.GetLength());
  return S_OK;
}

HRESULT BufferToString(const std::vector<byte>& buffer_in, CString* str_out) {
  ASSERT1(str_out);

  size_t len2 = buffer_in.size();
  ASSERT1(len2 % 2 == 0);
  size_t len = len2 / 2;

  str_out->Append(reinterpret_cast<const TCHAR*>(&buffer_in.front()), len);

  return S_OK;
}

HRESULT StringToBuffer(const CString& str_in, std::vector<byte>* buffer_out) {
  ASSERT1(buffer_out);

  size_t len = str_in.GetLength();
  size_t len2 = len * 2;

  buffer_out->resize(len2);
  ::memcpy(&buffer_out->front(), str_in.GetString(), len2);

  return S_OK;
}

HRESULT RegSplitKeyvalueName(const CString& keyvalue_name,
                             CString* key_name,
                             CString* value_name) {
  ASSERT1(key_name);
  ASSERT1(value_name);

  const TCHAR kDefault[] = _T("\\(default)");

  if (String_EndsWith(keyvalue_name, _T("\\"), false)) {
    key_name->SetString(keyvalue_name, keyvalue_name.GetLength() - 1);
    value_name->Empty();
  } else if (String_EndsWith(keyvalue_name, kDefault, true)) {
    key_name->SetString(keyvalue_name,
                        keyvalue_name.GetLength() - TSTR_SIZE(kDefault));
    value_name->Empty();
  } else {
    int last_slash = String_ReverseFindChar(keyvalue_name, _T('\\'));
    if (last_slash == -1) {
      // No slash found - bizzare and wrong
      return E_FAIL;
    }
    key_name->SetString(keyvalue_name, last_slash);
    value_name->SetString(keyvalue_name.GetString() + last_slash + 1,
                          keyvalue_name.GetLength() - last_slash - 1);
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
        marker_pos2 = s - src;
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
      UTIL_LOG(LEVEL_ERROR, (_T("[ExpandEnvLikeStrings]")
                             _T("[no mapping found for '%s' in '%s']"),
                             name, src));
      dest->Append(src + pos, marker_pos2 - pos + 1);
      hr = HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    pos = marker_pos2 + 1;
  }

  int len = _tcslen(src);
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

// Helper function to convert string to GUID
GUID StringToGuid(const CString& str) {
  GUID guid(GUID_NULL);
  if (!str.IsEmpty()) {
    TCHAR* s = const_cast<TCHAR*>(str.GetString());
    VERIFY(SUCCEEDED(::CLSIDFromString(s, &guid)), (_T("guid %s"), s));
  }
  return guid;
}

// Helper function to convert a variant containing a list of strings
void VariantToStringList(VARIANT var, std::vector<CString>* list) {
  ASSERT1(list);

  list->clear();

  ASSERT1(V_VT(&var) == VT_DISPATCH);
  CComPtr<IDispatch> obj = V_DISPATCH(&var);
  ASSERT1(obj);

  CComVariant var_length;
  VERIFY1(SUCCEEDED(obj.GetPropertyByName(_T("length"), &var_length)));
  ASSERT1(V_VT(&var_length) == VT_I4);
  int length = V_I4(&var_length);

  for (int i = 0; i < length; ++i) {
    CComVariant value;
    VERIFY1(SUCCEEDED(obj.GetPropertyByName(itostr(i), &value)));
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
    ASSERT(SUCCEEDED(set_value_hr), (_T("[GetLimitedTimeValue - failed ")
                                     _T("when setting a value][0x%x]"),
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
  ::RasEnumEntries(NULL,
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
  result.Format(_T("%s\\%s"), leftpart, rightpart);
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
  CORE_LOG(L3, (_T("[GetUserKeysFromHkeyUsers]")));

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
        CORE_LOG(LEVEL_WARNING,
            (_T("[GetUserKeysFromHkeyUsers LookupAccountSid] ")
             _T(" failed [0x%08x]"),
             hr));
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
    ASSERT(false, (_T("[CAccessToken::GetProcessToken 0x%x]"), hr));
    return hr;
  }
  CSid logon_sid;
  if (!current_process_token.GetUser(&logon_sid)) {
    HRESULT hr = HRESULTFromLastError();
    ASSERT(false, (_T("[CAccessToken::GetUser 0x%x]"), hr));
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

bool IsClickOnceDisabled() {
  CComPtr<IInternetZoneManager> zone_mgr;
  HRESULT hr =  zone_mgr.CoCreateInstance(CLSID_InternetZoneManager);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[CreateInstance InternetZoneManager failed][0x%x]"), hr));
    return true;
  }

  DWORD policy = URLPOLICY_DISALLOW;
  size_t policy_size = sizeof(policy);
  hr = zone_mgr->GetZoneActionPolicy(URLZONE_INTERNET,
                                     URLACTION_MANAGED_UNSIGNED,
                                     reinterpret_cast<BYTE*>(&policy),
                                     policy_size,
                                     URLZONEREG_DEFAULT);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetZoneActionPolicy failed][0x%x]"), hr));
    return true;
  }

  return policy == URLPOLICY_DISALLOW;
}

// This function only uses kernel32, and it is safe to call from DllMain.
HRESULT PinModuleIntoProcess(const CString& module_name) {
  ASSERT1(!module_name.IsEmpty());
  static HMODULE module_handle = NULL;
  typedef BOOL (WINAPI *Fun)(DWORD flags,
                             LPCWSTR module_name,
                             HMODULE* module_handle);

  HINSTANCE kernel_instance = ::GetModuleHandle(_T("kernel32.dll"));
  ASSERT1(kernel_instance);
  Fun pfn = NULL;
  if (GPA(kernel_instance, "GetModuleHandleExW", &pfn)) {
    if ((*pfn)(GET_MODULE_HANDLE_EX_FLAG_PIN, module_name, &module_handle)) {
      return S_OK;
    }
    ASSERT(false, (_T("GetModuleHandleExW() failed [%d]"), ::GetLastError()));
  }

  module_handle = ::LoadLibrary(module_name);
  ASSERT(NULL != module_handle, (_T("LoadLibrary [%d]"), ::GetLastError()));
  if (NULL == module_handle) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

bool ShellExecuteExEnsureParent(LPSHELLEXECUTEINFO shell_exec_info) {
  UTIL_LOG(L3, (_T("[ShellExecuteExEnsureParent]")));

  ASSERT1(shell_exec_info);
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
        UTIL_LOG(LE, (_T("[CreateDummyOverlappedWindow failed]")));
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
          UTIL_LOG(LW, (_T("[SetForegroundWindow fail %d]"), ::GetLastError()));
        }
      }
    }

    shell_exec_succeeded = !!::ShellExecuteEx(shell_exec_info);

    if (shell_exec_succeeded) {
      if (shell_exec_info->hProcess) {
        DWORD pid = Process::GetProcessIdFromHandle(shell_exec_info->hProcess);
        OPT_LOG(L1, (_T("[Started process][%u]"), pid));
        if (!::AllowSetForegroundWindow(pid)) {
          UTIL_LOG(LW, (_T("[AllowSetForegroundWindow %d]"), ::GetLastError()));
        }
      } else {
        OPT_LOG(L1, (_T("[Started process][PID unknown]")));
      }
    } else {
      last_error = ::GetLastError();
      UTIL_LOG(LE, (_T("[ShellExecuteEx fail][%s][%s][0x%08x]"),
                    shell_exec_info->lpFile, shell_exec_info->lpParameters,
                    last_error));
    }
  }

  // The implicit ::DestroyWindow call from the scoped_window could have reset
  // the last error, so restore it.
  ::SetLastError(last_error);

  return shell_exec_succeeded;
}

// Loads and unloads advapi32.dll for every call. If performance is an issue
// consider keeping the dll always loaded and holding the pointer to the
// RtlGenRandom in a static variable.
// Use the function with care. While the function is documented, it may be
// altered or made unavailable in future versions of the operating system.
bool GenRandom(void* buffer, size_t buffer_length) {
  ASSERT1(buffer);
  scoped_library lib(::LoadLibrary(_T("ADVAPI32.DLL")));
  if (lib) {
    typedef BOOLEAN (APIENTRY *RtlGenRandomType)(void*, ULONG);
    RtlGenRandomType rtl_gen_random = reinterpret_cast<RtlGenRandomType>(
        ::GetProcAddress(get(lib), "SystemFunction036"));
    return rtl_gen_random && rtl_gen_random(buffer, buffer_length);
  }

  // Use CAPI to generate randomness for systems which do not support
  // RtlGenRandomType, for instance Windows 2000.
  const uint32 kCspFlags = CRYPT_VERIFYCONTEXT | CRYPT_SILENT;
  HCRYPTPROV csp = NULL;
  if (::CryptAcquireContext(&csp, NULL, NULL, PROV_RSA_FULL, kCspFlags)) {
    if (::CryptGenRandom(csp, buffer_length, static_cast<BYTE*>(buffer))) {
      return true;
    }
  }
  VERIFY1(::CryptReleaseContext(csp, 0));
  return false;
}

// Assumes the path in command is properly enclosed if necessary.
HRESULT ConfigureRunAtStartup(const CString& root_key_name,
                              const CString& run_value_name,
                              const CString& command,
                              bool install) {
  UTIL_LOG(L3, (_T("ConfigureRunAtStartup")));

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
    UTIL_LOG(LE, (_T("[::CommandLineToArgvW failed][0x%x]"), hr));
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

CString GetEnvironmentVariableAsString(const TCHAR* name) {
  CString value;
  size_t value_length = ::GetEnvironmentVariable(name, NULL, 0);
  if (value_length) {
    VERIFY1(::GetEnvironmentVariable(name,
                                     CStrBuf(value, value_length),
                                     value_length));
  }
  return value;
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

}  // namespace omaha

