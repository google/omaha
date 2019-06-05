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


#include "omaha/base/app_util.h"
#include "omaha/base/environment_block_modifier.h"
#include "omaha/base/process.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/crashhandler/crash_dump_util_internal.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class CrashHandlerDumpUtilTest : public testing::Test {
 protected:
  CrashHandlerDumpUtilTest() {}
};

namespace {

int GenerateFakeMinidump(EXCEPTION_POINTERS* ex_info) {
  // Create a fake CustomInfo.
  google_breakpad::CustomInfoEntry entries[2] = {
      google_breakpad::CustomInfoEntry(L"value_name1", L"TestingValue"),
      google_breakpad::CustomInfoEntry(L"value_name2", L"CrashTest"),
      };
  google_breakpad::CustomClientInfo fake_custom_client_info = {
      entries, arraysize(entries) };

  // Create a fake MDRawAssertionInfo
  MDRawAssertionInfo fake_assert_info = {};
  lstrcpyn(reinterpret_cast<wchar_t*>(fake_assert_info.expression),
           L"FakeTestExpression",
           arraysize(fake_assert_info.expression));
  lstrcpyn(reinterpret_cast<wchar_t*>(fake_assert_info.function),
           L"FakeFunction()",
           arraysize(fake_assert_info.function));
  lstrcpyn(reinterpret_cast<wchar_t*>(fake_assert_info.file),
           L"FakeFile.cc",
           arraysize(fake_assert_info.file));
  fake_assert_info.line = 3344;
  fake_assert_info.type = 1;

  scoped_event fake_notification_event(::CreateEvent(NULL, TRUE, FALSE, NULL));
  DWORD thread_id = ::GetCurrentThreadId();
  google_breakpad::ClientInfo fake_client_info(
      NULL, ::GetCurrentProcessId(), MiniDumpNormal,
      &thread_id, &ex_info,
      &fake_assert_info, fake_custom_client_info);
  EXPECT_TRUE(fake_client_info.Initialize());

  bool is_system = false;
  CString crash_file;
  EXPECT_SUCCEEDED(GenerateMinidump(is_system, false, fake_client_info,
                                    &crash_file, NULL));
  EXPECT_TRUE(::DeleteFile(crash_file));

  EXPECT_SUCCEEDED(GenerateMinidump(is_system, true, fake_client_info,
                                    &crash_file, NULL));
  EXPECT_TRUE(::DeleteFile(crash_file));
  return EXCEPTION_EXECUTE_HANDLER;
}


// Create a process that runs as user who owns process identified by pid.
// All inheritable handles in current process will be accessible to the child
// process.
HRESULT CreateProcessForPidOwner(DWORD pid,
                                 const TCHAR* executable_path,
                                 const TCHAR* cmd_args,
                                 void* env_block,
                                 PROCESS_INFORMATION* pi) {
  ASSERT1(executable_path);

  // Find user token from the given process, then create a process with same
  // privilege.
  scoped_handle process_token;
  const DWORD token_flag = TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY;
  HRESULT hr(Process::GetPrimaryToken(pid, token_flag, address(process_token)));
  if (FAILED(hr)) {
    return hr;
  }

  const bool inherit_handles = true;
  hr = System::StartProcessAsUserWithEnvironment(get(process_token),
                                                 executable_path,
                                                 cmd_args,
                                                 NULL,
                                                 inherit_handles,
                                                 env_block,
                                                 pi);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Failed to create process for as process owner][%d][%s]"),
                  pid, executable_path));
    return hr;
  }

  CORE_LOG(L1, (_T("[CreateProcessForPidOwner succeeded.][command=%s]"),
                executable_path));
  return S_OK;
}

}  // namespace

TEST_F(CrashHandlerDumpUtilTest, WaitSubprocessWithInheritableEvent) {
  // Create an event so the dump process can notify.
  scoped_event notification_event;
  EXPECT_SUCCEEDED(CreateEventInheritableByProcessOwner(
      ::GetCurrentProcessId(), true, false, NULL, address(notification_event)));

  // Populate fake crash info.
  google_breakpad::CustomInfoEntry entries[2] = {
      google_breakpad::CustomInfoEntry(L"value_name1", L"TestingValue"),
      google_breakpad::CustomInfoEntry(L"value_name2", L"CrashTest"),
      };
  google_breakpad::CustomClientInfo fake_custom_client_info = {
      entries, arraysize(entries) };
  MDRawAssertionInfo assert_info;

  google_breakpad::ClientInfo fake_client_info(
      NULL, ::GetCurrentProcessId(), MiniDumpWithFullMemoryInfo,
      reinterpret_cast<DWORD*>(0x76543210),
      NULL, &assert_info, fake_custom_client_info);

  // Mock minidump generator exe path.
  const TCHAR kRelativePath[] = _T("MockMinidumper.exe");
  CString mock_dumper_path(app_util::GetCurrentModuleDirectory());
  ASSERT_TRUE(::PathAppend(CStrBuf(mock_dumper_path, MAX_PATH),
                           kRelativePath));

  // Pass crash process information via environment variables.
  EnvironmentBlockModifier eb_mod;
  EXPECT_SUCCEEDED(SetCrashInfoToEnvironmentBlock(&eb_mod,
                                                  get(notification_event),
                                                  fake_client_info));
  std::vector<TCHAR> env_block;
  EXPECT_TRUE(eb_mod.CreateForCurrentUser(&env_block));

  // Base 64 encode the crash info to a string.
  internal::CrashInfo fake_crash_info = {};
  fake_crash_info.crash_process_id = fake_client_info.pid();
  fake_crash_info.crash_thread_id = fake_client_info.thread_id();
  fake_crash_info.ex_info = fake_client_info.ex_info();
  fake_crash_info.assert_info = fake_client_info.assert_info();
  fake_crash_info.dump_type = fake_client_info.dump_type();
  fake_crash_info.custom_info.count = fake_custom_client_info.count;
  fake_crash_info.custom_info.entries = fake_custom_client_info.entries;
  CStringA encoded_crash_info;
  Base64Escape(reinterpret_cast<const char*>(&fake_crash_info),
               static_cast<int>(sizeof(fake_crash_info)),
               &encoded_crash_info,
               true);
  CString cmd_args;
  cmd_args.Format(
      _T("%s EventHandle=%p CrashInfo=%s"),
      _T("WaitSubprocessWithInheritableEvent"),
      get(notification_event),
      CString(encoded_crash_info));

  // Create a minidump generation process with given privilege and environment.
  PROCESS_INFORMATION pi = {};
  EXPECT_SUCCEEDED(CreateProcessForPidOwner(::GetCurrentProcessId(),
                                            mock_dumper_path,
                                            cmd_args,
                                            &env_block.front(),
                                            &pi));

  const DWORD kWaitTimeoutMs = 15 * 1000;   // 15 seconds
  HANDLE handles_to_wait[] = { get(notification_event), pi.hProcess };
  DWORD wait_result = ::WaitForMultipleObjects(arraysize(handles_to_wait),
                                               handles_to_wait,
                                               FALSE,
                                               kWaitTimeoutMs);
  // We expect the notification event is set before the child process exits.
  EXPECT_EQ(WAIT_OBJECT_0, wait_result);
}

TEST_F(CrashHandlerDumpUtilTest, GenerateMinidump) {
  __try {
    // Generate a divide by 0 exception. Copied from
    // crash_handler_client_tool.cc.
#pragma warning(push)
#pragma warning(disable : 4723)   // C4723: potential divide by 0
    volatile int foo = 10;
    volatile int bar = foo - 10;
    volatile int baz = foo / bar;
#pragma warning(pop)
  } __except(GenerateFakeMinidump(GetExceptionInformation())) {
  }
}

}  // namespace omaha

