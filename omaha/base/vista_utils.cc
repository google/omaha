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

#include "omaha/base/vista_utils.h"

#include <memory>
#include <vector>

#include "omaha/base/browser_utils.h"
#include "omaha/base/commontypes.h"
#include "omaha/base/const_utils.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/proc_utils.h"
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/smart_handle.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/system.h"
#include "omaha/base/system_info.h"
#include "omaha/base/user_info.h"
#include "omaha/base/user_rights.h"
#include "omaha/base/utils.h"
#include "omaha/third_party/smartany/scoped_any.h"

#define LOW_INTEGRITY_SID_W           NOTRANSL(L"S-1-16-4096")

namespace omaha {

namespace vista {

namespace {

// TODO(Omaha): Unit test for this method.
HRESULT RunAsUser(const CString& command_line,
                  HANDLE user_token,
                  bool run_as_current_user,
                  HANDLE* child_stdout,
                  HANDLE* process) {
  if (INVALID_HANDLE_VALUE == user_token) {
    return E_INVALIDARG;
  }

  CString cmd(command_line);

  STARTUPINFO startup_info = { sizeof(startup_info) };
  PROCESS_INFORMATION process_info = {0};
  BOOL inherit_handles = FALSE;
  scoped_handle pipe_read;
  scoped_handle pipe_write;

  if (child_stdout) {
    HRESULT hr = System::CreateChildOutputPipe(address(pipe_read),
                                               address(pipe_write));
    if (FAILED(hr)) {
      UTIL_LOG(LW, (_T("[CreateChildOutputPipe failed][0x%x]"), hr));
      return hr;
    }
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = get(pipe_write);
    startup_info.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
    inherit_handles = TRUE;
  }

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
      ::CreateProcess(0, CStrBuf(cmd, MAX_PATH), 0, 0, inherit_handles,
                      creation_flags, environment_block, 0, &startup_info,
                      &process_info) :
      ::CreateProcessAsUser(user_token, 0, CStrBuf(cmd, MAX_PATH), 0, 0,
                            inherit_handles, creation_flags, environment_block,
                            0, &startup_info, &process_info);

  if (!success) {
    HRESULT hr(HRESULTFromLastError());
    UTIL_LOG(LE, (_T("[RunAsUser failed][cmd=%s][hresult=0x%x]"), cmd, hr));
    return hr;
  }

  VERIFY1(::CloseHandle(process_info.hThread));
  if (!process) {
    VERIFY1(::CloseHandle(process_info.hProcess));
  } else {
    *process = process_info.hProcess;
  }

  if (child_stdout) {
    *child_stdout = release(pipe_read);
  }

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

  std::unique_ptr<TOKEN_MANDATORY_LABEL> integration_level;

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

HRESULT RunAsCurrentUser(const CString& command_line,
                         HANDLE* child_stdout,
                         HANDLE* process) {
  scoped_handle token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_DUPLICATE,
                          address(token))) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[RunAsCurrentUser: OpenProcessToken failed][0x%x]"), hr));
    return hr;
  }

  return RunAsUser(command_line, get(token), true,
                   child_stdout, process);
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
  omaha::user_info::GetProcessUser(NULL, NULL, &user_sid);
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

