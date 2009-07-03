// Copyright 2006-2009 Google Inc.
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


#include "omaha/common/vistautil.h"

#include <accctrl.h>
#include <Aclapi.h>
#include <Sddl.h>
#include <ShellAPI.h>
#include <shlobj.h>
#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/process.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/utils.h"
#include "omaha/common/vista_utils.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace vista_util {

static SID_IDENTIFIER_AUTHORITY mandatory_label_auth =
    SECURITY_MANDATORY_LABEL_AUTHORITY;


static HRESULT GetSidIntegrityLevel(PSID sid, MANDATORY_LEVEL* level) {
  if (!IsValidSid(sid))
    return E_FAIL;

  SID_IDENTIFIER_AUTHORITY* authority = GetSidIdentifierAuthority(sid);
  if (!authority)
    return E_FAIL;

  if (memcmp(authority, &mandatory_label_auth,
      sizeof(SID_IDENTIFIER_AUTHORITY)))
    return E_FAIL;

  PUCHAR count = GetSidSubAuthorityCount(sid);
  if (!count || *count != 1)
    return E_FAIL;

  DWORD* rid = GetSidSubAuthority(sid, 0);
  if (!rid)
    return E_FAIL;

  if ((*rid & 0xFFF) != 0 || *rid > SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
    return E_FAIL;

  *level = static_cast<MANDATORY_LEVEL>(*rid >> 12);
  return S_OK;
}

// Will return S_FALSE and MandatoryLevelMedium if the acl is NULL
static HRESULT GetAclIntegrityLevel(PACL acl, MANDATORY_LEVEL* level,
    bool* and_children) {
  *level = MandatoryLevelMedium;
  if (and_children)
    *and_children = false;
  if (!acl) {
    // This is the default label value if the acl was empty
    return S_FALSE;
  }

  SYSTEM_MANDATORY_LABEL_ACE* mandatory_label_ace;
  if (!GetAce(acl, 0, reinterpret_cast<void**>(&mandatory_label_ace)))
    return S_FALSE;

  if (mandatory_label_ace->Header.AceType != SYSTEM_MANDATORY_LABEL_ACE_TYPE)
    return S_FALSE;

  if (!(mandatory_label_ace->Mask & SYSTEM_MANDATORY_LABEL_NO_WRITE_UP)) {
    // I have found that if this flag is not set, a low integrity label doesn't
    // prevent writes from being virtualized.  MS provides zero documentation.
    // I just did an MSDN search, a Google search, and a search of the Beta
    // Vista SDKs, and no docs. TODO(omaha): Check docs again periodically.
    // For now, act as if no label was set, and default to medium.
    return S_FALSE;
  }

  if (and_children) {
    *and_children = ((mandatory_label_ace->Header.AceFlags &
        (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE))
        == (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE));
  }

  return GetSidIntegrityLevel(reinterpret_cast<SID*>(&mandatory_label_ace->
      SidStart), level);
}

// If successful, the caller needs to free the ACL using LocalFree()
// on failure, returns NULL
static ACL* CreateMandatoryLabelAcl(MANDATORY_LEVEL level, bool and_children) {
  int ace_size = sizeof(SYSTEM_MANDATORY_LABEL_ACE)
      - sizeof(DWORD) + GetSidLengthRequired(1);
  int acl_size = sizeof(ACL) + ace_size;

  ACL* acl = reinterpret_cast<ACL*>(LocalAlloc(LPTR, acl_size));
  if (!acl)
    return NULL;

  bool failed = true;
  if (InitializeAcl(acl, acl_size, ACL_REVISION)) {
    if (level > 0) {
      SYSTEM_MANDATORY_LABEL_ACE* ace = reinterpret_cast<
          SYSTEM_MANDATORY_LABEL_ACE*>(LocalAlloc(LPTR, ace_size));
      if (ace) {
        ace->Header.AceType = SYSTEM_MANDATORY_LABEL_ACE_TYPE;
        ace->Header.AceFlags = and_children ?
            (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) : 0;
        ace->Header.AceSize = static_cast<WORD>(ace_size);
        ace->Mask = SYSTEM_MANDATORY_LABEL_NO_WRITE_UP;

        SID* sid = reinterpret_cast<SID*>(&ace->SidStart);

        if (InitializeSid(sid, &mandatory_label_auth, 1)) {
          *GetSidSubAuthority(sid, 0) = static_cast<DWORD>(level) << 12;
          failed = !AddAce(acl, ACL_REVISION, 0, ace, ace_size);
        }
        LocalFree(ace);
      }
    }
  }
  if (failed) {
    LocalFree(acl);
    acl = NULL;
  }
  return acl;
}


TCHAR* AllocFullRegPath(HKEY root, const TCHAR* subkey) {
  if (!subkey)
    return NULL;

  const TCHAR* root_string;

  if (root == HKEY_CURRENT_USER)
    root_string = _T("CURRENT_USER\\");
  else if (root == HKEY_LOCAL_MACHINE)
    root_string = _T("MACHINE\\");
  else if (root == HKEY_CLASSES_ROOT)
    root_string = _T("CLASSES_ROOT\\");
  else if (root == HKEY_USERS)
    root_string = _T("USERS\\");
  else
    return NULL;

  size_t root_size = _tcslen(root_string);
  size_t size = root_size + _tcslen(subkey) + 1;
  TCHAR* result = reinterpret_cast<TCHAR*>(LocalAlloc(LPTR,
      size * sizeof(TCHAR)));
  if (!result)
    return NULL;

  memcpy(result, root_string, size * sizeof(TCHAR));
  memcpy(result + root_size, subkey, (1 + size - root_size) * sizeof(TCHAR));
  return result;
}


bool IsUserNonElevatedAdmin() {
  // If pre-Vista return false;
  if (!IsVistaOrLater()) {
    return false;
  }

  bool non_elevated_admin = false;
  scoped_handle token;
  if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_READ, address(token))) {
    TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
    DWORD infoLen = 0;
    if (::GetTokenInformation(get(token),
                              TokenElevationType,
                              reinterpret_cast<void*>(&elevation_type),
                              sizeof(elevation_type),
                              &infoLen)) {
      if (elevation_type == TokenElevationTypeLimited) {
        non_elevated_admin = true;
      }
    }
  }

  return non_elevated_admin;
}

bool IsUserAdmin() {
  // Determine if the user is part of the adminstators group. This will return
  // true in case of XP and 2K if the user belongs to admin group. In case of
  // Vista, it only returns true if the admin is running elevated.
  SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
  PSID administrators_group = NULL;
  BOOL result = ::AllocateAndInitializeSid(&nt_authority,
                                           2,
                                           SECURITY_BUILTIN_DOMAIN_RID,
                                           DOMAIN_ALIAS_RID_ADMINS,
                                           0, 0, 0, 0, 0, 0,
                                           &administrators_group);
  if (result) {
    if (!::CheckTokenMembership(NULL, administrators_group, &result)) {
      result = false;
    }
    ::FreeSid(administrators_group);
  }
  return !!result;
}

bool IsVistaOrLater() {
  static bool known = false;
  static bool is_vista = false;
  if (!known) {
    OSVERSIONINFOEX osvi = { 0 };
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    osvi.dwMajorVersion = 6;
    DWORDLONG conditional = 0;
    VER_SET_CONDITION(conditional, VER_MAJORVERSION, VER_GREATER_EQUAL);
    is_vista = !!VerifyVersionInfo(&osvi, VER_MAJORVERSION, conditional);
    // If the Win32 API failed for some other reason, callers may incorrectly
    // perform non-Vista operations. Assert we don't see any other failures.
    ASSERT1(is_vista || ERROR_OLD_WIN_VERSION == ::GetLastError());
    known = true;
  }
  return is_vista;
}

bool IsUserRunningSplitToken() {
  if (!IsVistaOrLater()) {
    return false;
  }

  scoped_handle process_token;
  if (!::OpenProcessToken(GetCurrentProcess(),
                          TOKEN_QUERY,
                          address(process_token))) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    UTIL_LOG(L1, (_T("[OpenProcessToken failed][0x%x]"), hr));
    return false;
  }

  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(get(process_token),
                             TokenElevationType,
                             &elevation_type,
                             sizeof(elevation_type),
                             &size_returned)) {
    HRESULT hr = HRESULT_FROM_WIN32(::GetLastError());
    UTIL_LOG(L1, (_T("[GetTokenInformation failed][0x%x]"), hr));
    return false;
  }

  return (elevation_type == TokenElevationTypeFull ||
          elevation_type == TokenElevationTypeLimited);
}

bool IsUACDisabled() {
  if (!IsVistaOrLater()) {
    return false;
  }

  if (IsUserRunningSplitToken()) {
    // Split token indicates that UAC is on.
    return false;
  }

  const TCHAR* key_name = _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\")
                          _T("CurrentVersion\\Policies\\System");

  DWORD val = 1;    // Assume UAC is enabled.
  return SUCCEEDED(RegKey::GetValue(key_name, _T("EnableLUA"), &val)) && !val;
}


HRESULT RunElevated(const TCHAR* file_path,
                    const TCHAR* parameters,
                    int show_window,
                    DWORD* exit_code) {
  OPT_LOG(L1, (_T("[Running elevated][%s][%s]"), file_path, parameters));

  SHELLEXECUTEINFO shell_execute_info;
  shell_execute_info.cbSize = sizeof(SHELLEXECUTEINFO);
  shell_execute_info.fMask = SEE_MASK_FLAG_NO_UI     |
                             SEE_MASK_NOZONECHECKS   |
                             SEE_MASK_NOASYNC;
  if (exit_code != NULL) {
    shell_execute_info.fMask |= SEE_MASK_NOCLOSEPROCESS;
  }
  shell_execute_info.hProcess = NULL;
  shell_execute_info.hwnd = NULL;
  shell_execute_info.lpVerb = L"runas";
  shell_execute_info.lpFile = file_path;
  shell_execute_info.lpParameters = parameters;
  shell_execute_info.lpDirectory = NULL;
  shell_execute_info.nShow = show_window;
  shell_execute_info.hInstApp = NULL;

  if (!ShellExecuteExEnsureParent(&shell_execute_info)) {
    return AtlHresultFromLastError();
  }

  scoped_process process(shell_execute_info.hProcess);

  // Wait for the end of the spawned process, if needed
  if (exit_code) {
    WaitForSingleObject(get(process), INFINITE);
    VERIFY1(GetExitCodeProcess(get(process), exit_code));
    UTIL_LOG(L1, (_T("[Elevated process exited][PID: %u][exit code: %u]"),
                  Process::GetProcessIdFromHandle(get(process)), *exit_code));
  } else {
    UTIL_LOG(L1, (_T("[Elevated process exited][PID: %u]"),
                  Process::GetProcessIdFromHandle(get(process))));
  }

  return S_OK;
}


HRESULT GetProcessIntegrityLevel(DWORD process_id, MANDATORY_LEVEL* level) {
  if (!IsVistaOrLater())
    return E_NOTIMPL;

  if (process_id == 0)
    process_id = GetCurrentProcessId();

  HRESULT result = E_FAIL;
  HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id);
  if (process != NULL) {
    HANDLE current_token;
    if (OpenProcessToken(process,
                         TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                         &current_token)) {
      DWORD label_size = 0;
      TOKEN_MANDATORY_LABEL* label;
      GetTokenInformation(current_token, TokenIntegrityLevel,
          NULL, 0, &label_size);
      if (label_size && (label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>
          (LocalAlloc(LPTR, label_size))) != NULL) {
        if (GetTokenInformation(current_token, TokenIntegrityLevel,
            label, label_size, &label_size)) {
          result = GetSidIntegrityLevel(label->Label.Sid, level);
        }
        LocalFree(label);
      }
      CloseHandle(current_token);
    }
    CloseHandle(process);
  }
  return result;
}


HRESULT GetFileOrFolderIntegrityLevel(const TCHAR* file,
    MANDATORY_LEVEL* level, bool* and_children) {
  if (!IsVistaOrLater())
    return E_NOTIMPL;

  PSECURITY_DESCRIPTOR descriptor;
  PACL acl = NULL;

  DWORD result = GetNamedSecurityInfo(const_cast<TCHAR*>(file), SE_FILE_OBJECT,
      LABEL_SECURITY_INFORMATION, NULL, NULL, NULL, &acl, &descriptor);
  if (result != ERROR_SUCCESS)
    return HRESULT_FROM_WIN32(result);

  HRESULT hr = GetAclIntegrityLevel(acl, level, and_children);
  LocalFree(descriptor);
  return hr;
}


HRESULT SetFileOrFolderIntegrityLevel(const TCHAR* file,
    MANDATORY_LEVEL level, bool and_children) {
  if (!IsVistaOrLater())
    return E_NOTIMPL;

  ACL* acl = CreateMandatoryLabelAcl(level, and_children);
  if (!acl)
    return E_FAIL;

  DWORD result = SetNamedSecurityInfo(const_cast<TCHAR*>(file), SE_FILE_OBJECT,
      LABEL_SECURITY_INFORMATION, NULL, NULL, NULL, acl);
  LocalFree(acl);
  return HRESULT_FROM_WIN32(result);
}


HRESULT GetRegKeyIntegrityLevel(HKEY root, const TCHAR* subkey,
    MANDATORY_LEVEL* level, bool* and_children) {
  if (!IsVistaOrLater())
    return E_NOTIMPL;

  TCHAR* reg_path = AllocFullRegPath(root, subkey);
  if (!reg_path)
    return E_FAIL;

  PSECURITY_DESCRIPTOR descriptor;
  PACL acl = NULL;

  DWORD result = GetNamedSecurityInfo(reg_path, SE_REGISTRY_KEY,
      LABEL_SECURITY_INFORMATION, NULL, NULL, NULL, &acl, &descriptor);
  if (result != ERROR_SUCCESS) {
    LocalFree(reg_path);
    return HRESULT_FROM_WIN32(result);
  }

  HRESULT hr = GetAclIntegrityLevel(acl, level, and_children);
  LocalFree(descriptor);
  LocalFree(reg_path);
  return hr;
}


HRESULT SetRegKeyIntegrityLevel(HKEY root, const TCHAR* subkey,
    MANDATORY_LEVEL level, bool and_children) {
  if (!IsVistaOrLater())
    return E_NOTIMPL;

  TCHAR* reg_path = AllocFullRegPath(root, subkey);
  if (!reg_path)
    return E_FAIL;

  ACL* acl = CreateMandatoryLabelAcl(level, and_children);
  if (!acl) {
    LocalFree(reg_path);
    return E_FAIL;
  }

  DWORD result = SetNamedSecurityInfo(reg_path, SE_REGISTRY_KEY,
      LABEL_SECURITY_INFORMATION, NULL, NULL, NULL, acl);
  LocalFree(acl);
  LocalFree(reg_path);
  return HRESULT_FROM_WIN32(result);
}


CSecurityDesc* BuildSecurityDescriptor(const TCHAR* sddl_sacl,
                                       ACCESS_MASK mask) {
  if (!IsVistaOrLater()) {
    return NULL;
  }

  scoped_ptr<CSecurityDesc> security_descriptor(new CSecurityDesc);
  security_descriptor->FromString(sddl_sacl);

  // Fill out the rest of the security descriptor from the process token.
  CAccessToken token;
  if (!token.GetProcessToken(TOKEN_QUERY)) {
    return NULL;
  }

  // The owner.
  CSid sid_owner;
  if (!token.GetOwner(&sid_owner)) {
    return NULL;
  }
  security_descriptor->SetOwner(sid_owner);

  // The group.
  CSid sid_group;
  if (!token.GetPrimaryGroup(&sid_group)) {
    return NULL;
  }
  security_descriptor->SetGroup(sid_group);

  // The discretionary access control list.
  CDacl dacl;
  if (!token.GetDefaultDacl(&dacl)) {
    return NULL;
  }

  // Add an access control entry mask for the current user.
  // This is what grants this user access from lower integrity levels.
  CSid sid_user;
  if (!token.GetUser(&sid_user)) {
    return NULL;
  }

  if (!dacl.AddAllowedAce(sid_user, mask)) {
    return NULL;
  }

  // Lastly, save the dacl to this descriptor.
  security_descriptor->SetDacl(dacl);
  return security_descriptor.release();
};

CSecurityDesc* CreateLowIntegritySecurityDesc(ACCESS_MASK mask) {
  return BuildSecurityDescriptor(LOW_INTEGRITY_SDDL_SACL, mask);
}

CSecurityDesc* CreateMediumIntegritySecurityDesc(ACCESS_MASK mask) {
  return BuildSecurityDescriptor(MEDIUM_INTEGRITY_SDDL_SACL, mask);
}

}  // namespace vista_util

}  // namespace omaha

