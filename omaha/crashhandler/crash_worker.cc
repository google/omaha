// Copyright 2014 Google Inc.
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

#include "omaha/crashhandler/crash_worker.h"

#include <memory>

#include "omaha/base/environment_block_modifier.h"
#include "omaha/base/error.h"
#include "omaha/base/process.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/crash_utils.h"
#include "omaha/crashhandler/crash_dump_util.h"
#include "omaha/crashhandler/crash_dump_util_internal.h"
#include "omaha/crashhandler/crashhandler_metrics.h"
#include "third_party/breakpad/src/client/windows/crash_generation/minidump_generator.h"

namespace omaha {

// This global tracks whether we have attempted to sandbox the worker process.
// It does not guarantee that sandboxing was successful.
// TODO(cdn) Move this and other non-utility code into crash_worker.cc
bool g_lockdown_complete = false;

const TCHAR* const kUntrustedIntegrityLevelString = _T("S-1-16-0");

// Sandbox the worker process before generating the minidumps.
HRESULT InitializeSandbox() {
  InitializeWorkerDesktop();

  // Setting integrity labels is only supported in Windows vista and above.
  // Pre-Vista installs do not get a sandbox.
  if (!SystemInfo::IsRunningOnVistaOrLater()) {
    return S_OK;
  }

  scoped_hlocal integrity_sid(NULL);
  if (!::ConvertStringSidToSid(kUntrustedIntegrityLevelString,
                               address(integrity_sid))) {
    return E_FAIL;
  }

  TOKEN_MANDATORY_LABEL label = {0};
  label.Label.Attributes = SE_GROUP_INTEGRITY;
  label.Label.Sid = static_cast<PSID>(get(integrity_sid));
  DWORD size = sizeof(TOKEN_MANDATORY_LABEL) +
      ::GetLengthSid(get(integrity_sid));
  scoped_handle token;
  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, address(token));
  BOOL result = ::SetTokenInformation(get(token),
                                      TokenIntegrityLevel,
                                      &label,
                                      size);
  return result ? S_OK : E_FAIL;
}

HRESULT InitializeWorkerDesktop() {
  scoped_hwinsta worker_window_station(::CreateWindowStation(NULL,
                                                             0,
                                                             WINSTA_ALL_ACCESS,
                                                             NULL));
  if (!worker_window_station)
    return E_FAIL;

  scoped_hwinsta original_window_station(::GetProcessWindowStation());
  scoped_hdesk original_desktop(::GetThreadDesktop(::GetCurrentThreadId()));
  if (!original_window_station ||
      !::SetProcessWindowStation(get(worker_window_station))) {
    return E_FAIL;
  }

  scoped_hdesk worker_desktop(
      ::CreateDesktop(CRASH_HANDLER_NAME _T("WorkerDesktop"),
                      NULL,
                      NULL,
                      0,
                      GENERIC_ALL,
                      NULL));
  if (!worker_desktop) {
    ::SetProcessWindowStation(get(original_window_station));
    return E_FAIL;
  }

  if (!::SetThreadDesktop(get(worker_desktop))) {
    ::SetProcessWindowStation(get(original_window_station));
    return E_FAIL;
  }

  return S_OK;
}

BOOL CALLBACK MinidumpStatusCallback(PVOID param,
                                     const PMINIDUMP_CALLBACK_INPUT input,
                                     PMINIDUMP_CALLBACK_OUTPUT output) {
  if (!input || !output) {
    return false;
  }

  if (g_lockdown_complete) {
    return true;
  }

  // Once the include module callback fires MinidumpWriteDump has finished
  // opening the handles it needs for dump generation. We can then initialize
  // the sandbox and add additional context to the dump.
  if (input->CallbackType != IncludeModuleCallback) {
    return true;
  }

  MinidumpCallbackParameter* callback_param =
      reinterpret_cast<MinidumpCallbackParameter*>(param);
  CrashAnalyzer* crash_analyzer = callback_param->crash_analyzer;
  std::map<CString, CString>* custom_info_map = callback_param->custom_info_map;
  MINIDUMP_USER_STREAM_INFORMATION* user_streams = callback_param->user_streams;
  MINIDUMP_USER_STREAM* user_stream_array = user_streams->UserStreamArray;

  // Drop the privileges of the crash handler worker.
  g_lockdown_complete = true;
  if (FAILED(InitializeSandbox())) {
    // If sandbox initialization fails we do not run the analyzer but still
    // allow dump generation to continue;
    return true;
  }

  if (!crash_analyzer) {
    // If we do not have an analyzer there is nothing left to do in the callback
    // Allow dump generation to continue.
    return true;
  }

  CrashAnalysisResult analysis_result = crash_analyzer->Analyze();
  if (analysis_result != ANALYSIS_NORMAL) {
    (*custom_info_map)[CString("CrashAnalysisResult")] =
        CrashAnalysisResultToString(analysis_result);

    user_streams->UserStreamCount = static_cast<ULONG>(
        crash_analyzer->GetUserStreamInfo(user_stream_array, kMaxUserStreams));
  }

  return true;
}

// Generates minidump for the given crash client.
HRESULT GenerateMinidump(bool is_system,
                         const google_breakpad::ClientInfo& client_info,
                         HANDLE mini_dump_file_handle,
                         HANDLE full_dump_file_handle,
                         MinidumpCallbackParameter* callback_param) {
  ASSERT1(mini_dump_file_handle);
  ASSERT1(client_info.pid() != 0);
  ASSERT1(client_info.process_handle());

  UNREFERENCED_PARAMETER(is_system);

  // We have to get the address of EXCEPTION_INFORMATION from
  // the client process address space.
  EXCEPTION_POINTERS* client_ex_info = NULL;
  if (!client_info.GetClientExceptionInfo(&client_ex_info)) {
    CORE_LOG(LE, (_T("[CrashHandler][GetClientExceptionInfo failed]")));
    return E_FAIL;
  }

  DWORD client_thread_id = 0;
  if (!client_info.GetClientThreadId(&client_thread_id)) {
    CORE_LOG(LE, (_T("[CrashHandler][GetClientThreadId failed]")));
    return E_FAIL;
  }
  // Passing an empty string as the dump directory is ok here because we are
  // generating the dumps using previously opened handles.
  std::unique_ptr<google_breakpad::MinidumpGenerator> dump_generator(
      new google_breakpad::MinidumpGenerator(std::wstring(),
                                             client_info.process_handle(),
                                             client_info.pid(),
                                             client_thread_id,
                                             GetCurrentThreadId(),
                                             client_ex_info,
                                             client_info.assert_info(),
                                             client_info.dump_type(),
                                             true));
  dump_generator->SetDumpFile(mini_dump_file_handle);
  dump_generator->SetFullDumpFile(full_dump_file_handle);

  std::unique_ptr<MINIDUMP_USER_STREAM[]> user_streams(
      new MINIDUMP_USER_STREAM[kMaxUserStreams]);
  memset(user_streams.get(), 0, sizeof(MINIDUMP_USER_STREAM) * kMaxUserStreams);

  MINIDUMP_USER_STREAM_INFORMATION additional_streams = {0};
  additional_streams.UserStreamArray = user_streams.get();
  additional_streams.UserStreamCount = kMaxUserStreams;
  dump_generator->SetAdditionalStreams(&additional_streams);

  MINIDUMP_CALLBACK_INFORMATION callback_info = {0};
  callback_info.CallbackRoutine = MinidumpStatusCallback;
  callback_param->user_streams = &additional_streams;
  callback_info.CallbackParam = reinterpret_cast<PVOID>(callback_param);
  dump_generator->SetCallback(&callback_info);

  if (!dump_generator->WriteMinidump()) {
    CORE_LOG(LE, (_T("[CrashHandler][WriteMinidump failed]")));
    return E_FAIL;
  }

  return S_OK;
}

HRESULT OpenCustomMapFile(const CString& dump_file,
                          HANDLE* custom_info_file_handle) {
  HRESULT hr = crash_utils::OpenCustomInfoFile(dump_file,
                                               custom_info_file_handle);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[OpenCustomInfoFile failed][0x%x]"), hr));
  }

  return hr;
}

// Generates custom map file for the given crash client.
HRESULT WriteCustomMapFile(DWORD crash_id,
                           google_breakpad::ClientInfo* client_info,
                           bool* is_uploaded_deferral_requested,
                           HANDLE custom_info_file_handle,
                           CrashAnalysisResult analysis_result) {
  UNREFERENCED_PARAMETER(crash_id);
  if (!client_info->PopulateCustomInfo()) {
    CORE_LOG(LE, (_T("[CrashHandler][PopulateCustomInfo failed]")));
    return E_FAIL;
  }

  std::map<CString, CString> custom_info_map;
  HRESULT hr = crash_utils::ConvertCustomClientInfoToMap(
      client_info->GetCustomInfo(), &custom_info_map);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ConvertCustomClientInfoToMap failed][0x%x]"), hr));
    ++metric_oop_crashes_convertcustomclientinfotomap_failed;
    ASSERT1(custom_info_map.empty());
    return hr;
  }
  custom_info_map[CString("CrashAnalysisResult")] =
      CrashAnalysisResultToString(analysis_result);
  *is_uploaded_deferral_requested =
      crash_utils::IsUploadDeferralRequested(custom_info_map);
  if (*is_uploaded_deferral_requested) {
    CORE_LOG(L1, (_T("[CrashHandler][Upload deferred][Crash ID %d]"),
                  crash_id));
    ++metric_oop_crashes_deferred;
  }

  hr = crash_utils::WriteCustomInfoFile(custom_info_file_handle,
                                        custom_info_map);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[WriteCustomInfoFile failed][0x%x]"), hr));
    ++metric_oop_crashes_createcustominfofile_failed;
    return hr;
  }

  CORE_LOG(L1, (_T("[Successfully created custom map file]")));
  return S_OK;
}

}  // namespace omaha
