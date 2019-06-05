// Copyright 2009-2010 Google Inc.
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
//
// A test installer that allows for running arbitrary code on a target machine,
// either via downloaded .bat files, or using any other mechanism provided the
// corresponding files are already on the machine.
//
// Usage is as follows:
// TestOmahaExeInstaller.exe {arg1} {arg2} {/installerdata=file.dat}
//
// Example usage 1:
// TestOmahaExeInstaller.exe c:\python24\python.exe c:\omahatestdir\foo.py
// In this case, TestOmahaExeInstaller.exe will run the following command line
//   c:\python24\python.exe c:\omahatestdir\foo.py
// The TestOmahaExeInstaller.exe process exit code will be whatever python.exe
// returns.
//
// Example usage 2:
// TestOmahaExeInstaller.exe /installerdata=file.dat
// In this case, file.dat will be interpreted as file.bat and executed. An
// example batch file is as follows:
//   set ARGS=/f /v cmd_line /t REG_SZ /d hello
//   reg add HKCU\Software\Google\FastSearch %ARGS%
//   exit 111
// This will write "hello" under the cmd_line REG_SZ value.
// The TestOmahaExeInstaller.exe process exit code will be 111.
//
// Example usage 3:
// TestOmahaExeInstaller.exe foo bar /installerdata=file.dat
// In this case, file.dat will be interpreted as file.bat and executed. An
// example batch file is as follows:
//   set ARGS=/f /v cmd_line /t REG_SZ /d %1%2
//   reg add HKCU\Software\Google\FastSearch %ARGS%
//   exit 222
// This will write "foobar" under the cmd_line REG_SZ value.
// The TestOmahaExeInstaller.exe process exit code will be 222.
//
// To pass in .bat files, the file should be checked in under the "data"
// directory in the Omaha server config, and the Omaha metainstaller should be
// tagged with the "installdataindex" that corresponding to that .bat file.

#include <windows.h>
#include <shellapi.h>
#include <tchar.h>

#include <atlpath.h>

#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/string.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace {

// Returns the filename corresponding to the kCmdLineInstallerData switch. This
// switch is always the last argument in cmd_line. If no kCmdLineInstallerData
// switch is found, returns a blank CString.
CString GetInstallerDataFilename(const CString& cmd_line) {
  int arg_value_start = cmd_line.Find(omaha::kCmdLineInstallerData);
  if (-1 == arg_value_start) {
    return CString();
  }

  arg_value_start += static_cast<int>(_tcslen(omaha::kCmdLineInstallerData));
  return cmd_line.Mid(arg_value_start);
}

// Returns a command line where the kCmdLineInstallerData switch is removed.
// This switch is always the last argument in cmd_line. If no
// kCmdLineInstallerData switch is found, returns cmd_line unaltered.
CString RemoveInstallerDataArgument(const CString& cmd_line) {
  int arg_value_start = cmd_line.Find(omaha::kCmdLineInstallerData);
  if (-1 == arg_value_start) {
    return cmd_line;
  }

  return cmd_line.Left(arg_value_start);
}

}  // namespace

int WINAPI _tWinMain(HINSTANCE, HINSTANCE, LPTSTR cmd_line, int) {
  int argument_count = 0;
  LPTSTR* argument_list = ::CommandLineToArgvW(cmd_line, &argument_count);
  scoped_hlocal scoped_argument_list(argument_list);

  if (!argument_list || argument_count < 1) {
    return E_INVALIDARG;
  }

  CPath data_file(
      GetInstallerDataFilename(argument_list[argument_count - 1]));
  data_file.UnquoteSpaces();

  CPath bat_file;
  if (data_file.FileExists()) {
    bat_file = data_file;
    bat_file.RemoveExtension();
    bat_file.AddExtension(_T(".bat"));

    HRESULT hr = omaha::File::Copy(data_file, bat_file, true);
    if (FAILED(hr)) {
      return hr;
    }

    bat_file.QuoteSpaces();
  }

  CString cmd_line_to_execute = RemoveInstallerDataArgument(cmd_line);

  // Run using "cmd.exe /C".
  const TCHAR kExecutableName[] = _T("cmd.exe");
  CString path = omaha::ConcatenatePath(omaha::app_util::GetSystemDir(),
                                        kExecutableName);
  omaha::Process p(path, NULL);

  CString command_line;
  command_line.Format(_T("/C %s %s"), bat_file, cmd_line_to_execute);
  HRESULT hr = p.Start(command_line, NULL);
  if (FAILED(hr)) {
    return hr;
  }

  p.WaitUntilDead(INFINITE);
  uint32 exit_code(0);
  if (!p.GetExitCode(&exit_code)) {
    return E_FAIL;
  }

  return exit_code;
}

