// Copyright 2011 Google Inc.
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

#include "omaha/goopdate/app_command.h"

#include <io.h>
#include <cstdio>
#include <atlstr.h>
#include <atlsimpstr.h>
#include <windows.h>
#include <algorithm>

#include "base/utils.h"
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/scope_guard.h"
#include "omaha/goopdate/app_command_verifier.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

const TCHAR* const kBadCmdLine = _T("cmd_garbeldy_gook.exe");
const TCHAR* const kCmdLineExit0 = _T("cmd.exe /c \"exit 0\"");
const TCHAR* const kCmdLineExit3 = _T("cmd.exe /c \"exit 3\"");
const TCHAR* const kCmdLineExitX = _T("cmd.exe /c \"exit %1\"");
const TCHAR* const kCmdLineSleep1 =
    _T("cmd.exe /c \"ping.exe 2.2.2.2 -n 1 -w 1000 >NUL\"");
const TCHAR* const kCmdLineEchoHelloWorldAscii =
    _T("cmd.exe /a /c \"echo Hello World\"");
const TCHAR* const kCmdLineEchoHelloWorldUnicode =
    _T("cmd.exe /u /c \"echo Hello World\"");
const TCHAR* const kCmdLineEchoWithSleep =
    _T("cmd.exe /c \"echo Hello World& ping.exe 2.2.2.2 -n 1 -w 1000 >NUL & ")
    _T("echo Goodbye World\"");

CString GetEchoCommandLine(CString string, CString output_file) {
  CString command_line;
  _sntprintf_s(CStrBuf(command_line, MAX_PATH),
               MAX_PATH,
               _TRUNCATE,
               _T("cmd.exe /c \"echo %s > \"%s\"\""),
               static_cast<const TCHAR*>(string),
               static_cast<const TCHAR*>(output_file));
  return command_line;
}

class MockAppCommandVerifier : public AppCommandVerifier {
 public:
  explicit MockAppCommandVerifier(HRESULT result) : result_(result) {}

  virtual HRESULT VerifyExecutable(const CString& path) {
    verified_path_ = path;
    return result_;
  }

  const CString& verified_path() { return verified_path_; }

 private:
  CString verified_path_;
  HRESULT result_;
};

}  // namespace

TEST(AppCommandTest, Constructor) {
  AppCommand app_command(kCmdLineExit0, true, false, false, false, NULL);
  ASSERT_TRUE(app_command.is_web_accessible());
  // TODO(erikwright): other accessors, variations.
}

TEST(AppCommandTest, Execute) {
  CString temp_file = GetTempFilename(_T("omaha"));
  ASSERT_FALSE(temp_file.IsEmpty());

  // GetTempFilename created an empty file. Delete it.
  ASSERT_EQ(0, _tunlink(temp_file));

  // Hopefully we will cause the file to be created. Cause its deletion at exit.
  ON_SCOPE_EXIT(_tunlink, temp_file);

  CString command_line = GetEchoCommandLine(_T("hello world!"), temp_file);

  AppCommand app_command(command_line, false, false, true, false, NULL);

  ASSERT_EQ(COMMAND_STATUS_INIT, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());

  scoped_process process;

  MockAppCommandVerifier fail_verifier(E_FAIL);
  ASSERT_FAILED(app_command.Execute(&fail_verifier,
                                    std::vector<CString>(),
                                    address(process)));

  MockAppCommandVerifier succeed_verifier(S_OK);
  ASSERT_SUCCEEDED(app_command.Execute(&succeed_verifier,
                                       std::vector<CString>(),
                                       address(process)));
  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));

  ASSERT_EQ(COMMAND_STATUS_COMPLETE, app_command.GetStatus());
  ASSERT_EQ(0, app_command.GetExitCode());

  ASSERT_TRUE(File::Exists(temp_file));

  ASSERT_EQ(_T("cmd.exe"), fail_verifier.verified_path());
  ASSERT_EQ(_T("cmd.exe"), succeed_verifier.verified_path());
}

TEST(AppCommandTest, NoDefaultCapture) {
  AppCommand app_command(
      kCmdLineEchoHelloWorldAscii, false, false, false, false, NULL);

  scoped_process process;

  ASSERT_SUCCEEDED(app_command.Execute(NULL,
                                       std::vector<CString>(),
                                       address(process)));
  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));

  ASSERT_EQ(COMMAND_STATUS_COMPLETE, app_command.GetStatus());
  ASSERT_EQ(0, app_command.GetExitCode());

  ASSERT_EQ(CString(), app_command.GetOutput());
}

TEST(AppCommandTest, CaptureOutputAscii) {
  AppCommand app_command(kCmdLineEchoHelloWorldAscii,
                         false, false, true, false, NULL);

  scoped_process process;

  ASSERT_SUCCEEDED(app_command.Execute(NULL,
                                       std::vector<CString>(),
                                       address(process)));
  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));

  ASSERT_EQ(COMMAND_STATUS_COMPLETE, app_command.GetStatus());
  ASSERT_EQ(0, app_command.GetExitCode());

  ASSERT_EQ(CString(_T("Hello World\r\n")), app_command.GetOutput());
}

TEST(AppCommandTest, CaptureOutputTwoReads) {
  AppCommand app_command(kCmdLineEchoWithSleep,
                         false, false, true, false, NULL);

  scoped_process process;

  ASSERT_SUCCEEDED(app_command.Execute(NULL,
                                       std::vector<CString>(),
                                       address(process)));
  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));

  ASSERT_EQ(COMMAND_STATUS_COMPLETE, app_command.GetStatus());
  ASSERT_EQ(0, app_command.GetExitCode());

  ASSERT_EQ(CString(_T("Hello World\r\nGoodbye World\r\n")),
            app_command.GetOutput());
}

TEST(AppCommandTest, CaptureOutputUnicode) {
  AppCommand app_command(
      kCmdLineEchoHelloWorldUnicode, false, false, true, false, NULL);

  scoped_process process;

  ASSERT_SUCCEEDED(app_command.Execute(NULL,
                                       std::vector<CString>(),
                                       address(process)));
  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));

  ASSERT_EQ(COMMAND_STATUS_COMPLETE, app_command.GetStatus());
  ASSERT_EQ(0, app_command.GetExitCode());

  ASSERT_EQ(CString(_T("Hello World\r\n")), app_command.GetOutput());
}

TEST(AppCommandTest, ExecuteParameterizedCommand) {
  AppCommand app_command(kCmdLineExitX, false, false, false, false, NULL);

  scoped_process process;
  std::vector<CString> parameters;
  parameters.push_back(_T("3"));
  MockAppCommandVerifier succeed_verifier(S_OK);
  ASSERT_SUCCEEDED(app_command.Execute(&succeed_verifier,
                                        parameters,
                                        address(process)));
  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));
  DWORD exit_code;
  ASSERT_TRUE(::GetExitCodeProcess(get(process), &exit_code));
  ASSERT_EQ(3, exit_code);
  ASSERT_EQ(_T("cmd.exe"), succeed_verifier.verified_path());
}

TEST(AppCommandTest, FailedToLaunchStatus) {
  AppCommand app_command(kBadCmdLine, false, false, false, false, NULL);

  ASSERT_EQ(COMMAND_STATUS_INIT, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());

  scoped_process process;
  MockAppCommandVerifier success_verifier(S_OK);
  ASSERT_FAILED(app_command.Execute(&success_verifier,
                                     std::vector<CString>(),
                                     address(process)));
  ASSERT_EQ(COMMAND_STATUS_INIT, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());
}

TEST(AppCommandTest, CommandFailureStatus) {
  AppCommand app_command(kCmdLineExit3, false, false, false, false, NULL);

  ASSERT_EQ(COMMAND_STATUS_INIT, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());

  scoped_process process;
  MockAppCommandVerifier success_verifier(S_OK);
  ASSERT_SUCCEEDED(app_command.Execute(&success_verifier,
                                        std::vector<CString>(),
                                        address(process)));
  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));

  ASSERT_EQ(COMMAND_STATUS_COMPLETE, app_command.GetStatus());
  ASSERT_EQ(3, app_command.GetExitCode());
}

TEST(AppCommandTest, CommandRunningStatus) {
  AppCommand app_command(kCmdLineSleep1, false, false, false, false, NULL);

  ASSERT_EQ(COMMAND_STATUS_INIT, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());

  scoped_process process;
  MockAppCommandVerifier success_verifier(S_OK);
  ASSERT_SUCCEEDED(app_command.Execute(&success_verifier,
                                       std::vector<CString>(),
                                       address(process)));
  // If this ever fails because the status is COMMAND_STATUS_COMPLETE,
  // try increasing the time that the command sleeps from 1000 ms.
  ASSERT_EQ(COMMAND_STATUS_RUNNING, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());

  ASSERT_TRUE(app_command.Join(16 * kMsPerSec));

  ASSERT_EQ(COMMAND_STATUS_COMPLETE, app_command.GetStatus());
  ASSERT_EQ(1, app_command.GetExitCode());
}

TEST(AppCommandTest, AutoRunOnOSUpgradeCommand) {
  AppCommand app_command(kCmdLineExit3, false, false, false, true, NULL);

  ASSERT_EQ(COMMAND_STATUS_INIT, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());

  scoped_process process;
  MockAppCommandVerifier success_verifier(S_OK);
  ASSERT_SUCCEEDED(app_command.Execute(&success_verifier,
                                       std::vector<CString>(),
                                       address(process)));
  ASSERT_EQ(COMMAND_STATUS_INIT, app_command.GetStatus());
  ASSERT_EQ(MAXDWORD, app_command.GetExitCode());
}

}  // namespace omaha
