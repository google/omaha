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

#include "omaha/crashhandler/crash_analyzer.h"
#include "omaha/testing/unit_test.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

namespace omaha {

std::wstring kDebugeeExecutable = L"CrashHandlerTestDebugee.exe";

CrashAnalyzer* InitializeCrashAnalyzer(std::wstring test_name) {
  HANDLE ready_event = ::CreateEvent(NULL, TRUE, FALSE,
                                     L"CrashAnalyzerDebugeeReady");
  PROCESS_INFORMATION process_info = {0};
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  std::wstring cmd_line(kDebugeeExecutable +
                        L" CrashAnalyzerDebugeeReady " +
                        test_name);
  if (!::CreateProcess(NULL, const_cast<LPWSTR>(cmd_line.c_str()),
                       NULL, NULL, FALSE, NULL, NULL, NULL,
                       &startup_info, &process_info)) {
    return NULL;
  }
  google_breakpad::CustomClientInfo custom_client_info = {0};
  // The address of the EXCEPTION_POINTERS is hardcoded in the test debugee.
  // We do this so that we can mock the exception record without hooking up
  // all of the plumbing involved in causing a real exception.
  google_breakpad::ClientInfo* client_info =
      new google_breakpad::ClientInfo(
          NULL, process_info.dwProcessId, static_cast<MINIDUMP_TYPE>(0),
          NULL, reinterpret_cast<EXCEPTION_POINTERS**>(0x41410000), NULL,
          custom_client_info);
  client_info->Initialize();
  CrashAnalyzer* analyzer = new CrashAnalyzer(*client_info);
  ::WaitForSingleObject(ready_event, INFINITE);
  ::ResetEvent(ready_event);
  ::CloseHandle(ready_event);
  analyzer->Init();
  return analyzer;
}

void CleanupCrashAnalyzer(CrashAnalyzer* analyzer) {
  TerminateProcess(analyzer->client_info().process_handle(), NULL);
  delete &analyzer->client_info();
  delete analyzer;
}

TEST(CrashAnalyzerTest, Normal) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"Normal");
  EXPECT_EQ(ANALYSIS_NORMAL, analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

TEST(CrashAnalyzerTest, MaxExecMappings) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"MaxExecMappings");
  EXPECT_EQ(ANALYSIS_EXCESSIVE_EXEC_MEM,
            analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

TEST(CrashAnalyzerTest, NtFunctionsOnStack) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"NtFunctionsOnStack");
  EXPECT_EQ(ANALYSIS_NORMAL,
            analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

TEST(CrashAnalyzerTest, WildStackPointer) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"WildStackPointer");
  EXPECT_EQ(ANALYSIS_WILD_STACK_PTR,
            analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

TEST(CrashAnalyzerTest, PENotInModuleList) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"PENotInModuleList");
  EXPECT_EQ(ANALYSIS_BAD_IMAGE_MAPPING,
            analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

TEST(CrashAnalyzerTest, ShellcodeSprayPattern) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"ShellcodeSprayPattern");
  EXPECT_EQ(ANALYSIS_FOUND_SHELLCODE,
            analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

TEST(CrashAnalyzerTest, ShellcodeJmpCallPop) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"ShellcodeJmpCallPop");
  EXPECT_EQ(ANALYSIS_FOUND_SHELLCODE,
            analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

TEST(CrashAnalyzerTest, TiBDereference) {
  CrashAnalyzer* analyzer = InitializeCrashAnalyzer(L"TiBDereference");
  EXPECT_EQ(ANALYSIS_FAILED_TIB_DEREF,
            analyzer->Analyze());
  CleanupCrashAnalyzer(analyzer);
}

}  // namespace omaha
