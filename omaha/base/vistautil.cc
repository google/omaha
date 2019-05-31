// Copyright 2006-2010 Google Inc.
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

#include "omaha/base/vistautil.h"

#include <accctrl.h>
#include <Aclapi.h>
#include <Sddl.h>
#include <ShellAPI.h>
#include <shlobj.h>
#include <memory>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace vista_util {

namespace {

struct free_deleter{
  template <typename T>
  void operator()(T *p) const {
    std::free(const_cast<std::remove_const_t<T>*>(p));
  }
};

template <typename T>
using scoped_ptr_malloc = std::unique_ptr<T,free_deleter>;
static_assert(sizeof(char *)==sizeof(scoped_ptr_malloc<char>),"");

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

  PUCHAR count = ::GetSidSubAuthorityCount(sid);
  if (!count || *count != 1)
    return E_FAIL;

  DWORD* rid = ::GetSidSubAuthority(sid, 0);
  if (!rid)
    return E_FAIL;

  if ((*rid & 0xFFF) != 0 || *rid > SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
    return E_FAIL;

  *level = static_cast<MANDATORY_LEVEL>(*rid >> 12);
  return S_OK;
}

// Determine the mandatory level of a process
//   processID, the process to query, or (0) to use the current process
//   On Vista, level should alwys be filled in with either
//     MandatoryLevelLow (IE)
//     MandatoryLevelMedium(user), or
//     MandatoryLevelHigh( Elevated Admin)
//   On error, level remains unchanged
HRESULT GetProcessIntegrityLevel(DWORD process_id, MANDATORY_LEVEL* level) {
  if (!IsVistaOrLater())
    return E_NOTIMPL;

  if (process_id == 0)
    process_id = ::GetCurrentProcessId();

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

}  // namespace

// If successful, the caller needs to free the ACL using free().
// On failure, returns NULL.
static ACL* CreateMandatoryLabelAcl(MANDATORY_LEVEL level, bool and_children) {
  const WORD ace_size = offsetof(SYSTEM_MANDATORY_LABEL_ACE, SidStart)
      + static_cast<WORD>(::GetSidLengthRequired(1));
  const WORD acl_size = sizeof(ACL) + ace_size;

  scoped_ptr_malloc<ACL> acl(static_cast<ACL*>(malloc(acl_size)));
  if (!acl.get()) {
    return NULL;
  }

  if (!::InitializeAcl(acl.get(), acl_size, ACL_REVISION)) {
    return NULL;
  }

  scoped_ptr_malloc<SYSTEM_MANDATORY_LABEL_ACE> ace(static_cast<
      SYSTEM_MANDATORY_LABEL_ACE*>(malloc(ace_size)));
  if (!ace.get()) {
    return NULL;
  }

  ace->Header.AceType = SYSTEM_MANDATORY_LABEL_ACE_TYPE;
  ace->Header.AceFlags = and_children ?
      (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE) : 0;
  ace->Header.AceSize = ace_size;
  ace->Mask = SYSTEM_MANDATORY_LABEL_NO_WRITE_UP;

  SID* sid = reinterpret_cast<SID*>(&ace->SidStart);
  if (!::InitializeSid(sid, &mandatory_label_auth, 1)) {
    return NULL;
  }

  *::GetSidSubAuthority(sid, 0) = MANDATORY_LEVEL_TO_MANDATORY_RID(level);
  if (!::AddAce(acl.get(), ACL_REVISION, 0, ace.get(), ace_size)) {
    return NULL;
  }

  return acl.release();
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
  return SystemInfo::IsRunningOnVistaOrLater();
}

HRESULT IsUserRunningSplitToken(bool* is_split_token) {
  ASSERT1(is_split_token);

  if (!IsVistaOrLater()) {
    *is_split_token = false;
    return S_OK;
  }

  scoped_handle process_token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY,
                          address(process_token))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(L1, (_T("[OpenProcessToken failed][0x%x]"), hr));
    return hr;
  }

  TOKEN_ELEVATION_TYPE elevation_type = TokenElevationTypeDefault;
  DWORD size_returned = 0;
  if (!::GetTokenInformation(get(process_token),
                             TokenElevationType,
                             &elevation_type,
                             sizeof(elevation_type),
                             &size_returned)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(L1, (_T("[GetTokenInformation failed][0x%x]"), hr));
    return hr;
  }

  *is_split_token = elevation_type == TokenElevationTypeFull ||
                    elevation_type == TokenElevationTypeLimited;
  ASSERT1(*is_split_token || elevation_type == TokenElevationTypeDefault);

  return S_OK;
}

HRESULT IsUACOn(bool* is_uac_on) {
  ASSERT1(is_uac_on);

  if (!vista_util::IsVistaOrLater()) {
    *is_uac_on = false;
    return S_OK;
  }

  // The presence of a split token definitively indicates that UAC is on. But
  // the absence does not necessarily indicate that UAC is off.
  bool is_split_token = false;
  if (SUCCEEDED(IsUserRunningSplitToken(&is_split_token)) && is_split_token) {
    *is_uac_on = true;
    return S_OK;
  }

  uint32 pid = 0;
  HRESULT hr = vista::GetExplorerPidForCurrentUserOrSession(&pid);
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[Could not get Explorer PID][%#x]"), hr));
    return hr;
  }

  MANDATORY_LEVEL integrity_level = MandatoryLevelUntrusted;
  hr = GetProcessIntegrityLevel(pid, &integrity_level);
  ASSERT(SUCCEEDED(hr), (_T("[%#x]"), hr));
  if (FAILED(hr)) {
    return hr;
  }

  *is_uac_on = integrity_level <= MandatoryLevelMedium;
  return S_OK;
}

HRESULT IsElevatedWithUACOn(bool* is_elevated_with_uac_on) {
  ASSERT1(is_elevated_with_uac_on);

  if (!IsUserAdmin() || !IsVistaOrLater()) {
    *is_elevated_with_uac_on = false;
    return S_OK;
  }

  return IsUACOn(is_elevated_with_uac_on);
}

bool IsEnableLUAOn() {
  ASSERT1(vista_util::IsVistaOrLater());

  const TCHAR* key_name = _T("HKLM\\SOFTWARE\\Microsoft\\Windows\\")
                          _T("CurrentVersion\\Policies\\System");
  DWORD enable_lua = 0;
  return FAILED(RegKey::GetValue(key_name, _T("EnableLUA"), &enable_lua)) ||
         enable_lua;
}

bool IsElevatedWithEnableLUAOn() {
  return IsUserAdmin() && IsVistaOrLater() && IsEnableLUAOn();
}

HRESULT RunElevated(const TCHAR* file_path,
                    const TCHAR* parameters,
                    int show_window,
                    DWORD* exit_code) {
  UTIL_LOG(L1, (_T("[Running elevated][%s][%s]"), file_path, parameters));

  ASSERT1(vista_util::IsVistaOrLater());
  ASSERT1(!vista_util::IsUserAdmin());

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

HRESULT SetMandatorySacl(MANDATORY_LEVEL level, CSecurityDesc* sd) {
  ASSERT1(sd);

  if (!IsVistaOrLater()) {
    return S_FALSE;
  }

  scoped_ptr_malloc<ACL> acl(CreateMandatoryLabelAcl(level, false));
  if (!acl.get()) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[Failed to create the mandatory SACL][%#x]"), hr));
    return hr;
  }

  // Atl::CSacl does not support SYSTEM_MANDATORY_LABEL_ACE_TYPE, so we use
  // ::SetSecurityDescriptorSacl() to set the SACL.
  CSacl sacl_empty;
  sd->SetSacl(sacl_empty);

  if (!::SetSecurityDescriptorSacl(
             const_cast<SECURITY_DESCRIPTOR*>(sd->GetPSECURITY_DESCRIPTOR()),
             TRUE,
             acl.get(),
             FALSE))  {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[Failed to set the mandatory SACL][%#x]"), hr));
    return hr;
  }

  acl.release();
  return S_OK;
}

HRESULT EnableProcessHeapMetadataProtection() {
  if (!IsVistaOrLater()) {
    return S_FALSE;
  }

  if (!::HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[Failed to enable heap metadata protection][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

}  // namespace vista_util

}  // namespace omaha

