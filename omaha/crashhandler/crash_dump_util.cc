// Copyright 2013 Google Inc.
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

#include "omaha/crashhandler/crash_dump_util.h"

#include <memory>

#include "omaha/base/environment_block_modifier.h"
#include "omaha/base/error.h"
#include "omaha/base/process.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/crash_utils.h"
#include "omaha/crashhandler/crash_dump_util_internal.h"
#include "omaha/crashhandler/crashhandler_metrics.h"
#include "third_party/breakpad/src/client/windows/crash_generation/minidump_generator.h"

namespace omaha {

namespace {

const TCHAR* const kLaunchedForMinidump = _T("CrashHandlerLaunchedForMinidump");
const TCHAR* const kCrashInfoKey = _T("CrashHandlerEnv_CrashInfo");

HRESULT SetValueToEnvironmentBlock(EnvironmentBlockModifier* eb_mod,
                                   const TCHAR* name,
                                   const void* value, size_t value_size) {
  ASSERT1(value);

  CStringA value_string_ascii;
  Base64Escape(static_cast<const char*>(value),
               static_cast<int>(value_size),
               &value_string_ascii,
               true);

  if (value_string_ascii.IsEmpty()) {
    CORE_LOG(LE, (_T("[SetValueToEnvironmentBlock failed.][%s=%s]"),
                  name, value));
    return E_INVALIDARG;
  }

  eb_mod->SetVar(name, CString(value_string_ascii));
  return S_OK;
}


HRESULT GetEnvironmentVariableToBuffer(const TCHAR* name,
                                       void* value_out,
                                       size_t value_buffer_size) {
  ASSERT1(name);
  ASSERT1(value_out);
  const CString env_value = GetEnvironmentVariableAsString(name);
  if (env_value.IsEmpty()) {
    CORE_LOG(LE, (_T("[Environment variable not defined.][%s]"), name));
    return E_FAIL;
  }

  const CStringA env_value_char(env_value);
  int result_size = Base64Unescape(env_value_char,
                                   env_value_char.GetLength(),
                                   static_cast<char*>(value_out),
                                   static_cast<int>(value_buffer_size));
  if (result_size < 0 ||
      value_buffer_size != static_cast<size_t>(result_size)) {
    CORE_LOG(LE, (_T("[Failed to decode value][%s=%s]"), name, env_value));
    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  return S_OK;
}

HRESULT GetCrashInfoFromEnv(internal::CrashInfo* crash_info) {
  ASSERT1(crash_info);
  ::ZeroMemory(crash_info, sizeof(*crash_info));

  HRESULT hr = S_OK;
  hr = GetEnvironmentVariableToBuffer(
      kCrashInfoKey,
      crash_info,
      sizeof(*crash_info));
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

}  // namespace

// Checks whether current process is created to handle a particular crash dump.
// If environment kLaunchedForMinidump is set, the process runs to generate a
// minidump.
bool IsRunningAsMinidumpHandler() {
  CString value = GetEnvironmentVariableAsString(kLaunchedForMinidump);
  return !value.IsEmpty();
}

// Assigns given handles and client info to an environment block.
HRESULT SetCrashInfoToEnvironmentBlock(
    EnvironmentBlockModifier* eb_mod,
    HANDLE notification_event,
    HANDLE mini_dump_handle,
    HANDLE full_dump_handle,
    HANDLE custom_info_handle,
    const google_breakpad::ClientInfo& client_info) {
  internal::CrashInfo crash_info = {};
  crash_info.notification_event = notification_event;
  crash_info.mini_dump_handle = mini_dump_handle;
  crash_info.full_dump_handle = full_dump_handle;
  crash_info.custom_info_handle = custom_info_handle;
  crash_info.crash_id = client_info.crash_id();
  crash_info.crash_process_id = client_info.pid();
  crash_info.crash_thread_id = client_info.thread_id();
  crash_info.ex_info = client_info.ex_info();
  crash_info.assert_info = client_info.assert_info();
  crash_info.dump_type = client_info.dump_type();
  crash_info.custom_info = client_info.custom_client_info();

  HRESULT hr = SetValueToEnvironmentBlock(
      eb_mod,
      kCrashInfoKey,
      reinterpret_cast<const char*>(&crash_info),
      sizeof(crash_info));
  CORE_LOG(L1, (_T("[SetValueToEnvironmentBlock][0x%x]"), hr));
  return hr;
}

// Gets notification event handles and client info value from environment
// variables. Note the pointer values are in crashed process memory space,
// do not dereference directly.
// Caller is responsible to free client_info object.
HRESULT GetCrashInfoFromEnvironmentVariables(
    HANDLE* notification_event,
    HANDLE* mini_dump_handle,
    HANDLE* full_dump_handle,
    HANDLE* custom_info_handle,
    std::unique_ptr<google_breakpad::ClientInfo>* client_info_ptr) {
  ASSERT1(notification_event);
  ASSERT1(mini_dump_handle);
  ASSERT1(full_dump_handle);
  ASSERT1(custom_info_handle);
  ASSERT1(client_info_ptr);
  *notification_event = NULL;
  client_info_ptr->reset();
  *mini_dump_handle = NULL;
  *full_dump_handle = NULL;
  *custom_info_handle = NULL;

  internal::CrashInfo crash_info = {};
  HRESULT hr = GetCrashInfoFromEnv(&crash_info);
  if (FAILED(hr)) {
    return hr;
  }

  *notification_event = crash_info.notification_event;
  *mini_dump_handle = crash_info.mini_dump_handle;
  *full_dump_handle = crash_info.full_dump_handle;
  *custom_info_handle = crash_info.custom_info_handle;
  std::unique_ptr<google_breakpad::ClientInfo> client_info;
  client_info.reset(new google_breakpad::ClientInfo(NULL,
                                                    crash_info.crash_process_id,
                                                    crash_info.dump_type,
                                                    crash_info.crash_thread_id,
                                                    crash_info.ex_info,
                                                    crash_info.assert_info,
                                                    crash_info.custom_info));
  if (!client_info->Initialize()) {
    CORE_LOG(LE, (_T("[CrashHandler][Failed to initialize ClientInfo.")));
    return E_FAIL;
  }
  client_info_ptr->reset(client_info.release());
  return S_OK;
}

}  // namespace omaha
