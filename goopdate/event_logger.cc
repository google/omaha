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


#include "omaha/goopdate/event_logger.h"

#include <sddl.h>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/user_info.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource_manager.h"

namespace omaha {

void LogEventHelper(WORD type, DWORD id, size_t count, const TCHAR** strings,
                    const TCHAR* ctx) {
  ASSERT1(count <= kint16max);
  if (!ConfigManager::Instance()->CanLogEvents(type)) {
    return;
  }

  // Include the circular logging buffer in the event log if the type is a
  // warning or an error.
  CStringA data(ctx);
  CString context = GetLogging()->GetHistory();
  if (!context.IsEmpty()) {
    data.AppendFormat("\n[More context: %S]", context);
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
  key_name.Format(_T("HKLM\\SYSTEM\\CurrentControlSet\\Services\\EventLog\\")
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
  int result = ::RegSetValueEx(reg_key.Key(),
                               _T("EventMessageFile"),
                               0,
                               REG_EXPAND_SZ,
                               reinterpret_cast<const byte*>(msg_dll_path),
                               (_tcslen(msg_dll_path) + 1) * sizeof(TCHAR));
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

  //  Opens the log on the local computer.
  HANDLE hlog = ::RegisterEventSource(NULL, src_name);
  if (!hlog) {
    return HRESULTFromLastError();
  }

  // Best effort to get the sid for the current user. The event logging
  // provides for logging the sid at no cost so that the user shows up
  // in the event log.
  CString sid_string;
  VERIFY1(SUCCEEDED(user_info::GetCurrentUser(NULL, NULL, &sid_string)));
  PSID psid = NULL;
  if (!sid_string.IsEmpty()) {
    VERIFY1(::ConvertStringSidToSid(sid_string, &psid));
    ASSERT1(psid);
  }

  HRESULT hr = E_FAIL;
  if (::ReportEvent(hlog,       // Event log handle.
                    type,       // Event type.
                    category,   // Event category.
                    id,         // Event identifier.
                    psid,       // User security identifier.
                    count,      // Number of substitution strings.
                    buf_size,   // Size of binary data.
                    strings,    // Pointer to strings.
                    buffer)) {  // Binary data.
    hr = S_OK;
  } else {
    hr = HRESULTFromLastError();
  }

  ::LocalFree(psid);
  VERIFY1(::DeregisterEventSource(hlog));
  return hr;
}

// TODO(omaha): When we do i18n later on, decide if the string below needs
// translation or not. On one hand this string makes it to the event viewer, on
// the other hand the same string is used to register an event log for an
// application in registry. We do not expect the mapping to change when the user
// changes languages, however we may decide to do so.
const TCHAR* const EventLogger::kSourceName = _T("Google Update");

void GoogleUpdateLogEvent::WriteEvent() {
  ASSERT1(!event_desc_.IsEmpty());
  ASSERT1(type_ != 0);
  ASSERT1(id_ != 0);

  CString ver;
  goopdate_utils::GetVerFromRegistry(is_machine_, kGoogleUpdateAppId, &ver);

  CString lang = ResourceManager::GetDefaultUserLanguage();

  const ConfigManager& cm = *ConfigManager::Instance();
  CString msg;
  msg.Format(_T("\n%s.\nver=%s, lang=%s, machine=%d, extern=%d"),
             event_desc_, ver, lang, is_machine_, !cm.IsGoogler());
#if DEBUG
  msg.Append(_T(", debug"));
#endif
#if !OFFICIAL_BUILD
  msg.Append(_T(", private"));
#endif

  if (!event_text_.IsEmpty()) {
    msg.AppendFormat(_T("\n%s"), event_text_);
  }

  LogEvent(static_cast<WORD>(type_), id_, msg);
}


}  // namespace omaha

