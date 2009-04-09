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

#include "omaha/common/vista_utils.h"

#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/const_utils.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/proc_utils.h"
#include "omaha/common/process.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/smart_handle.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/system.h"
#include "omaha/common/system_info.h"
#include "omaha/common/user_info.h"
#include "omaha/common/user_rights.h"
#include "omaha/common/utils.h"

#define LOW_INTEGRITY_SDDL_SACL_A     NOTRANSL("S:(ML;;NW;;;LW)")
#define LOW_INTEGRITY_SID_W           NOTRANSL(L"S-1-16-4096")

namespace omaha {

namespace vista {

namespace {

// TODO(Omaha): Unit test for this method.
HRESULT RunAsUser(const CString& command_line,
                  HANDLE user_token,
                  bool run_as_current_user) {
  if (INVALID_HANDLE_VALUE == user_token) {
    return E_INVALIDARG;
  }

  CString cmd(command_line);

  STARTUPINFO startup_info = { sizeof(startup_info) };
  PROCESS_INFORMATION process_info = {0};

  DWORD creation_flags(0);
  void* environment_block(NULL);
  ON_SCOPE_EXIT(::DestroyEnvironmentBlock, environment_block);
  if (::CreateEnvironmentBlock(&environment_block, user_token, FALSE)) {
    creation_flags |= CREATE_UNICODE_ENVIRONMENT;
  } else {
    ASSERT(false, (_T("::CreateEnvironmentBlock failed %d"), ::GetLastError()));
    environment_block = NULL;
  }

  // ::CreateProcessAsUser() does not work unless the caller is SYSTEM. Does not
  // matter if the user token is for the current user.
  BOOL success = run_as_current_user ?
      ::CreateProcess(0, CStrBuf(cmd, MAX_PATH), 0, 0, false, creation_flags,
                      environment_block, 0, &startup_info, &process_info) :
      ::CreateProcessAsUser(user_token, 0, CStrBuf(cmd, MAX_PATH), 0, 0, false,
                            creation_flags, environment_block, 0, &startup_info,
                            &process_info);

  if (!success) {
    HRESULT hr(HRESULTFromLastError());
    UTIL_LOG(LE, (_T("[RunAsUser failed][cmd=%s][hresult=0x%x]"), cmd, hr));
    return hr;
  }

  VERIFY1(::CloseHandle(process_info.hThread));
  VERIFY1(::CloseHandle(process_info.hProcess));

  return S_OK;
}

}  // namespace

bool IsProcessProtected() {
  if (!SystemInfo::IsRunningOnVistaOrLater()) {
    return false;
  }

  AutoHandle token;
  VERIFY1(::OpenProcessToken(GetCurrentProcess(),
                             TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                             &token.receive()));

  // Get the Integrity level.
  DWORD length_needed;
  BOOL b = ::GetTokenInformation(token,
                                 TokenIntegrityLevel,
                                 NULL,
                                 0,
                                 &length_needed);
  ASSERT1(b == FALSE);
  if (b) {
    return false;
  }

  // The first call to GetTokenInformation is just to get the buffer size
  if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return false;
  }

  scoped_ptr<TOKEN_MANDATORY_LABEL> integration_level;

  integration_level.reset(reinterpret_cast<TOKEN_MANDATORY_LABEL*>(
      new char[length_needed]));
  if (integration_level.get() == NULL) {
    return false;
  }

  if (!::GetTokenInformation(token,
                             TokenIntegrityLevel,
                             integration_level.get(),
                             length_needed,
                             &length_needed)) {
    return false;
  }

  wchar_t* sid_str = NULL;
  VERIFY1(::ConvertSidToStringSid(integration_level->Label.Sid, &sid_str));
  bool ret = ::lstrcmpW(sid_str, LOW_INTEGRITY_SID_W) == 0;
  ::LocalFree(sid_str);

  return ret;
}

HRESULT AllowProtectedProcessAccessToSharedObject(const TCHAR* name) {
  if (!SystemInfo::IsRunningOnVistaOrLater()) {
    return S_FALSE;
  }

  ASSERT1(name != NULL);

  PSECURITY_DESCRIPTOR psd = NULL;
  VERIFY1(::ConvertStringSecurityDescriptorToSecurityDescriptorA(
              LOW_INTEGRITY_SDDL_SACL_A,
              SDDL_REVISION_1,
              &psd,
              NULL));

  BOOL sacl_present = FALSE;
  BOOL sacl_defaulted = FALSE;
  PACL sacl = NULL;
  VERIFY1(::GetSecurityDescriptorSacl(psd,
                                      &sacl_present,
                                      &sacl,
                                      &sacl_defaulted));

  DWORD ret = ::SetNamedSecurityInfoW(const_cast<TCHAR*>(name),
                                      SE_KERNEL_OBJECT,
                                      LABEL_SECURITY_INFORMATION,
                                      NULL,
                                      NULL,
                                      NULL,
                                      sacl);

  ::LocalFree(psd);

  return HRESULT_FROM_WIN32(ret);
}

HRESULT RunAsCurrentUser(const CString& command_line) {
  scoped_handle token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_DUPLICATE,
                          address(token))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[RunAsCurrentUser: OpenProcessToken failed][0x%x]"), hr));
    return hr;
  }

  return RunAsUser(command_line, get(token), true);
}

static HRESULT StartInternetExplorerAsUser(HANDLE user_token,
                                           const CString& options) {
  // Internet Explorer path
  CString ie_file_path;
  HRESULT result = RegKey::GetValue(kRegKeyIeClass,
                                    kRegValueIeClass,
                                    &ie_file_path);
  ASSERT1(SUCCEEDED(result));

  if (SUCCEEDED(result)) {
    CString command_line(ie_file_path);
    command_line += _T(' ');
    command_line += options;
    UTIL_LOG(L5, (_T("[StartInternetExplorerAsUser]")
                  _T("[Running IExplore with command line][%s]"),
                  command_line));
    result = RunAsUser(command_line, user_token, false);
  }
  return result;
}

//
// Constants used by RestartIEUser()
//
// The IEUser executable name
const TCHAR* kIEUser = _T("IEUSER.EXE");

// The maximum number of simultaneous
// logged on users in FUS that we support
const int kMaximumUsers = 16;


// Restart IEUser processs. This is to allow for
// IEUser.exe to refresh it's ElevationPolicy cache. Due to a bug
// within IE7, IEUser.exe does not refresh it's cache unless it
// is restarted in the manner below. If the cache is not refreshed
// IEUser does not respect any new ElevationPolicies that a fresh
// setup program installs for an ActiveX control or BHO. This code
// is adapted from Toolbar.
HRESULT RestartIEUser() {
  // Use the service to restart IEUser.
  // This allows us to restart IEUser for:
  //   (a) Multiple users for the first-install case
  //       (we currently only restart IEUser for the current interactive user)
  //   (b) Even if we are started in an elevated mode

  if (!SystemInfo::IsRunningOnVistaOrLater()) {
    UTIL_LOG(L5, (_T("[RestartIEUser - not running on Vista - Exiting]")));
    return S_OK;
  }

  // The restart should be attempted from the system account
  bool is_system_process = false;
  if (FAILED(IsSystemProcess(&is_system_process)) || !is_system_process) {
    ASSERT1(false);
    return E_ACCESSDENIED;
  }

  // Get the list of users currently running IEUser.exe processes.
  scoped_handle ieuser_users[kMaximumUsers];
  int number_of_users = 0;
  Process::GetUsersOfProcesses(kIEUser, kMaximumUsers, ieuser_users,
                               &number_of_users);

  UTIL_LOG(L5, (_T("[RestartIEUser]")
                _T("[number_of_users running IEUser %d]"), number_of_users));

  if (!number_of_users) {
    UTIL_LOG(L5, (_T("[RestartIEUser][No IEUser processes running]")));
    return S_OK;
  }

  // Kill current IEUser processes.
  ProcessTerminator pt(kIEUser);
  const int kKillWaitTimeoutMs = 5000;
  bool found = false;
  const int kill_method = (ProcessTerminator::KILL_METHOD_4_TERMINATE_PROCESS);

  RET_IF_FAILED(pt.KillTheProcess(kKillWaitTimeoutMs,
                                  &found,
                                  kill_method,
                                  false));

  // Restart them.
  HRESULT result = S_OK;
  for (int i = 0; i < number_of_users; i++) {
    // To start a new ieuser.exe, simply start iexplore.exe as a normal user
    // The -embedding prevents IE from opening a window
    HRESULT restart_result = StartInternetExplorerAsUser(get(ieuser_users[i]),
                                                         _T("-embedding"));
    if (FAILED(restart_result)) {
      UTIL_LOG(LEVEL_ERROR, (_T("[StartInternetExplorerAsUser failed][0x%x]"),
                             restart_result));
      result = restart_result;
    }
  }

  return result;
}

HRESULT GetExplorerPidForCurrentUserOrSession(uint32* pid) {
  ASSERT1(pid);
  std::vector<uint32> pids;
  HRESULT hr = GetProcessPidsForActiveUserOrSession(kExplorer, &pids);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[Did not find explorer.exe processes][0x%x]"), hr));
    return hr;
  }

  CORE_LOG(L1, (_T("[Found %u instance(s) of explorer.exe]"), pids.size()));

  *pid = pids[0];   // Return only the first instance of explorer.exe.
  return S_OK;
}

HRESULT GetExplorerTokenForLoggedInUser(HANDLE* token) {
  UTIL_LOG(L3, (_T("[GetExplorerTokenForLoggedInUser]")));
  ASSERT1(token);

  // TODO(omaha): One can set the windows shell to be other than
  // explorer.exe, handle this case. One way to handle this is to
  // read the regkey
  // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon
  // The only problem with this is it can be overriden with the user reg keys.
  // Need to figure out a method to do this.
  // Also consider using the interactive user before picking the first explorer
  // process i.e. the active user.
  std::vector<uint32> processes;
  DWORD flags = EXCLUDE_CURRENT_PROCESS;
  std::vector<CString> command_lines;
  CString explorer_file_name(kExplorer);
  CString user_sid;

  HRESULT hr = Process::FindProcesses(flags,
                                      explorer_file_name,
                                      true,
                                      user_sid,
                                      command_lines,
                                      &processes);
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[FindProcesses failed][0x%08x]"), hr));
    return hr;
  }

  std::vector<uint32>::const_iterator iter = processes.begin();
  for (; iter != processes.end(); ++iter) {
    uint32 explorer_pid = *iter;
    scoped_handle exp(::OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                                    false,
                                    explorer_pid));
    if (exp) {
      if (::OpenProcessToken(get(exp),
                             TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_IMPERSONATE,
                             token)) {
        // TODO(omaha): Consider using the GetWindowsAccountDomainSid
        // method here. This method returns the domain SID associated
        // with the passed in SID. This allows us to detect if the user is a
        // domain user. We should prefer domain users over normal users,
        // as in corporate environments, these users will be more likely to
        // allow to be tunneled through a proxy.
        return S_OK;
      } else {
        hr = HRESULTFromLastError();
        CORE_LOG(LEVEL_WARNING, (_T("[OpenProcessToken failed][0x%08x]"), hr));
      }
    } else {
      hr = HRESULTFromLastError();
      CORE_LOG(LEVEL_WARNING, (_T("[OpenProcess failed][0x%08x]"), hr));
    }
  }

  return hr;
}

HRESULT GetPidsInSession(const TCHAR* exe_name,
                         const TCHAR* user_sid,
                         DWORD session_id,
                         std::vector<uint32>* pids) {
  ASSERT1(pids);
  ASSERT1(exe_name);
  ASSERT1(*exe_name);
  UTIL_LOG(L3, (_T("[GetPidsInSession][%s][sid=%s][session=%d]"),
                exe_name, user_sid, session_id));

  pids->clear();

  DWORD flags = EXCLUDE_CURRENT_PROCESS;
  if (user_sid != NULL) {
    flags |= INCLUDE_ONLY_PROCESS_OWNED_BY_USER;
  }
  std::vector<CString> command_lines;
  HRESULT hr = Process::FindProcessesInSession(session_id,
                                               flags,
                                               exe_name,
                                               true,
                                               user_sid,
                                               command_lines,
                                               pids);
  if (FAILED(hr)) {
    return hr;
  }
  return pids->empty() ? HRESULT_FROM_WIN32(ERROR_NOT_FOUND) : S_OK;
}

HRESULT GetProcessPidsForActiveUserOrSession(const TCHAR* exe_name,
                                             std::vector<uint32>* pids) {
  bool is_system = false;
  HRESULT hr = IsSystemProcess(&is_system);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[IsSystemProcess failed][0x%x]"), hr));
    return hr;
  }

  if (is_system) {
    return vista::GetPidsInSession(exe_name,
                                   NULL,
                                   System::GetActiveSessionId(),
                                   pids);
  }

  CString user_sid;
  // If this call fails, we are still ok.
  omaha::user_info::GetCurrentUser(NULL, NULL, &user_sid);
  DWORD current_session = System::GetCurrentSessionId();
  if (FAILED(vista::GetPidsInSession(exe_name,
                                     user_sid,
                                     current_session,
                                     pids))) {
    // In the case of RunAs, the processes may be under a different identity
    // than the current sid. So if we are unable to find a process under the
    // current user's sid, we search for processes running in the current
    // session regardless of the sid they are running under.
    return vista::GetPidsInSession(exe_name,
                                   NULL,
                                   current_session,
                                   pids);
  }

  return S_OK;
}



HRESULT StartProcessWithTokenOfProcess(uint32 pid,
                                       const CString& command_line) {
  UTIL_LOG(L5, (_T("[StartProcessWithTokenOfProcess]")
                _T("[pid %u][command_line '%s']"), pid, command_line));

  // Get the token from process.
  scoped_handle user_token;
  HRESULT hr = Process::GetImpersonationToken(pid, address(user_token));
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[GetImpersonationToken failed][0x%08x]"), hr));
    return hr;
  }

  // Start process using the token.
  UTIL_LOG(L5, (_T("[StartProcessWithTokenOfProcess][Running process %s]"),
                command_line));
  hr = RunAsUser(command_line, get(user_token), false);

  if (FAILED(hr)) {
    UTIL_LOG(LEVEL_ERROR,
      (_T("[Vista::StartProcessWithTokenOfProcess - RunAsUser failed][0x%x]"),
      hr));
  }

  return hr;
}

HRESULT GetLoggedOnUserToken(HANDLE* token) {
  ASSERT1(token);
  *token = NULL;

  uint32 pid = 0;
  HRESULT hr = GetExplorerPidForCurrentUserOrSession(&pid);
  if (FAILED(hr)) {
    return hr;
  }
  hr = Process::GetImpersonationToken(pid, token);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(*token);
  return S_OK;
}

}  // namespace vista

}  // namespace omaha

