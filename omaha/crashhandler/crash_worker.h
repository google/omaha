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

#ifndef OMAHA_CRASHHANDLER_CRASH_WORKER_H_
#define OMAHA_CRASHHANDLER_CRASH_WORKER_H_

#include <windows.h>

#include "base/basictypes.h"
#include "omaha/base/string.h"
#include "omaha/crashhandler/crash_analyzer.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

namespace omaha {

const size_t kMaxUserStreams = 100;
struct MinidumpCallbackParameter {
  CrashAnalyzer* crash_analyzer;
  std::map<CString, CString>* custom_info_map;
  MINIDUMP_USER_STREAM_INFORMATION* user_streams;
};

// Sets the untrusted integrity level label on the crash handler worker process.
HRESULT InitializeSandbox();

// Creates a new Window Station and Desktop, switches to them, and closes the
// originals.
HRESULT InitializeWorkerDesktop();

// Generates dump files for the given crash client.
// If extended_dump is set this will also append some additional information to
// the dump on top of what was requested by the client.
HRESULT GenerateMinidump(bool is_system,
                         const google_breakpad::ClientInfo& client_info,
                         HANDLE mini_dump_file_handle,
                         HANDLE full_dump_file_handle,
                         MinidumpCallbackParameter* callback_param);

// Opens a handle to the file which will be used for the custom data map.
HRESULT OpenCustomMapFile(const CString& dump_file,
                          HANDLE* custom_info_file_handle);

// Generates custom map file for the given crash client.
HRESULT GenerateCustomMapFile(DWORD crash_id,
                              const CString& crash_filename,
                              google_breakpad::ClientInfo* client_info,
                              bool* is_uploaded_deferral_requested,
                              CString* custom_info_filename,
                              CrashAnalysisResult analysis_result);

}  // namespace omaha

#endif  // OMAHA_CRASHHANDLER_CRASH_WORKER_H_
