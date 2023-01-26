// Copyright 2007-2009 Google Inc.
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


#include "omaha/common/event_logger.h"
#include <sddl.h>
#include <intsafe.h>
#include <stdint.h>
#include <limits>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/user_info.h"
#include "omaha/common/config_manager.h"

namespace omaha {

void LogEventHelper(WORD type, DWORD id, size_t count, const TCHAR** strings,
                    const TCHAR* ctx) {
  ASSERT1(count <= static_cast<size_t>(std::numeric_limits<int16_t>::max()));

  // Include the circular logging buffer in the event log if the type is a
  // warning or an error.
  CStringA data(ctx);
  CString context = GetLogging()->GetHistory();
  if (!context.IsEmpty()) {
    SafeCStringAAppendFormat(&data, "\n[More context: %S]", context);
  }

  CString joined_strings;
  for (size_t i = 0; i < count; ++i) {
    SafeCStringAppendFormat(&joined_strings, L"[%s]", strings[i]);
  }
  OPT_LOG(L1, (_T("[LogEventHelper][%d][%d][%d][%s][%S]"), type, id, count,
               joined_strings, data));
  if (!ConfigManager::Instance()->CanLogEvents(type)) {
    return;
  }

  HRESULT hr = EventLogger::ReportEvent(EventLogger::kSourceName,
                                        type,
                                        EventLogger::kDefaultCategory,
                                        id,
                                        static_cast<WORD>(count),
                                        strings,
                                        data.GetLength(),
                                        data.GetBuffer());
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[Failed to log event][0x%08x]"), hr));
  }
}

CString BuildEventSourceRegistryKeyName(const TCHAR* src_name) {
  ASSERT1(src_name);
  CString key_name;
  SafeCStringFormat(&key_name,
                    _T("HKLM\\SYSTEM\\CurrentControlSet\\Services\\EventLog\\")
                    _T("Application\\%s"),
                    src_name);
  return key_name;
}

HRESULT EventLogger::AddEventSource(const TCHAR* src_name,
                                    const TCHAR* msg_dll_path) {
  ASSERT1(src_name);
  if (!src_name) return E_INVALIDARG;
  ASSERT1(msg_dll_path);
  if (!msg_dll_path) return E_INVALIDARG;

  // Create the event source as a subkey of the "Application" log.
  RegKey reg_key;
  HRESULT hr = reg_key.Create(BuildEventSourceRegistryKeyName(src_name));
  if (FAILED(hr)) return hr;

  // Set the name of the message file. RegKey class can't set REG_EXPAND_SZ
  // values so we must use the low level OS call.
  int result = ::RegSetValueEx(
      reg_key.Key(),
      _T("EventMessageFile"),
      0,
      REG_EXPAND_SZ,
      reinterpret_cast<const byte*>(msg_dll_path),
      static_cast<DWORD>((_tcslen(msg_dll_path) + 1) * sizeof(TCHAR)));
  if (result != ERROR_SUCCESS) return HRESULT_FROM_WIN32(result);

  // Set the supported event types.
  DWORD types = EVENTLOG_ERROR_TYPE |
                EVENTLOG_WARNING_TYPE |
                EVENTLOG_INFORMATION_TYPE;
  hr = reg_key.SetValue(_T("TypesSupported"), types);
  if (FAILED(hr)) return hr;

  return S_OK;
}

HRESULT EventLogger::RemoveEventSource(const TCHAR* src_name) {
  ASSERT1(src_name);
  if (!src_name) return E_INVALIDARG;

  // RegKey::DeleteKey  returns S_FALSE when attempting to delete
  // a key that is not there.
  HRESULT hr = RegKey::DeleteKey(BuildEventSourceRegistryKeyName(src_name),
                                 false);
  return SUCCEEDED(hr) ? S_OK : hr;
}


HRESULT EventLogger::ReportEvent(const TCHAR* src_name,
                                 WORD type,
                                 WORD category,
                                 DWORD id,
                                 WORD count,
                                 const TCHAR** strings,
                                 size_t buf_size,
                                 void* buffer) {
  ASSERT1(src_name);
  ASSERT1(type == EVENTLOG_SUCCESS ||
          type == EVENTLOG_ERROR_TYPE ||
          type == EVENTLOG_WARNING_TYPE ||
          type == EVENTLOG_INFORMATION_TYPE);

  if (buf_size > DWORD_MAX) {
    return E_INVALIDARG;
  }

  //  Opens the log on the local computer.
  HANDLE hlog = ::RegisterEventSourceW(NULL, src_name);
  if (!hlog) {
    return HRESULTFromLastError();
  }

  // Best effort to get the sid for the current effective user. The event
  // logging provides for logging the sid at no cost so that the user shows up
  // in the event log.
  CString sid_string;
  VERIFY_SUCCEEDED(user_info::GetEffectiveUserSid(&sid_string));
  PSID psid = NULL;
  if (!sid_string.IsEmpty()) {
    VERIFY1(::ConvertStringSidToSid(sid_string, &psid));
    ASSERT1(psid);
  }

  HRESULT hr = E_FAIL;
  if (::ReportEvent(hlog,
                    type,
                    category,
                    id,
                    psid,
                    count,
                    static_cast<DWORD>(buf_size),
                    strings,
                    buffer)) {
    hr = S_OK;
  } else {
    hr = HRESULTFromLastError();
  }

  ::LocalFree(psid);
  VERIFY1(::DeregisterEventSource(hlog));
  return hr;
}

HRESULT EventLogger::ReadLastEvent(const TCHAR* src_name, EVENTLOGRECORD* rec) {
  if (!(rec && src_name)) {
    return E_INVALIDARG;
  }
  HANDLE hlog = ::OpenEventLog(NULL, src_name);
  if (!hlog) {
    return HRESULTFromLastError();
  }
  HRESULT hr = E_FAIL;
  DWORD bytes_read(0), bytes_needed(0);
  const DWORD read_flags = EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ;
  if (::ReadEventLog(hlog,              // Event log handle.
                     read_flags,        // Reverse chronological order.
                     0,                 // Not used.
                     rec,               // Read buffer.
                     rec->Length,       // Size of read buffer.
                     &bytes_read,       // Number of bytes read.
                     &bytes_needed)) {  // Number of bytes required.
    hr = S_OK;
  } else {
    hr = HRESULTFromLastError();
  }
  ::CloseEventLog(hlog);
  return hr;
}

// TODO(omaha): Decide whether to use IDS_PRODUCT_DISPLAY_NAME instead.
// On one hand this string makes it to the event viewer, on
// the other hand the same string is used to register an event log for an
// application in registry. We do not expect the mapping to change when the user
// changes languages or there are multiple users for a per-machine install that
// are using different languages., however we may decide to do so.
const TCHAR* const EventLogger::kSourceName = kAppName;

void GoogleUpdateLogEvent::WriteEvent() {
  ASSERT1(!event_desc_.IsEmpty());
  ASSERT1(type_ != 0);
  ASSERT1(id_ != 0);

  const DWORD pid(::GetCurrentProcessId());
  const TCHAR* ver = GetVersionString();

  const ConfigManager& cm = *ConfigManager::Instance();
  CString msg;
  SafeCStringFormat(&msg, _T("\n%s.\npid=%d, ver=%s, machine=%d, extern=%d"),
                    event_desc_, pid, ver, is_machine_, !cm.IsInternalUser());
#if DEBUG
  msg.Append(_T(", debug"));
#endif
#if !OFFICIAL_BUILD
  msg.Append(_T(", private"));
#endif

  if (!event_text_.IsEmpty()) {
    SafeCStringAppendFormat(&msg, _T("\n%s"), event_text_);
  }

  LogEvent(static_cast<WORD>(type_), id_, msg);
}

}  // namespace omaha

