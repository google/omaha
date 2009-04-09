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
//
// Some simple network diags.

#include "omaha/net/net_diags.h"
#include <windows.h>
#include <stdarg.h>
#include <vector>
#include "omaha/common/app_util.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/debug.h"
#include "omaha/common/system_info.h"
#include "omaha/common/utils.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"

namespace omaha {

NetDiags::NetDiags() {
  Initialize();
}

NetDiags::~NetDiags() {
  ::CoUninitialize();
}

bool PrintToConsole(const TCHAR* format, ...) {
  ASSERT1(format);
  va_list arg_list;
  va_start(arg_list, format);
  CString msg;
  msg.FormatV(format, arg_list);
  ASSERT1(msg.GetLength() > 0);
  va_end(arg_list);

  DWORD count = 0;
  bool result = !!::WriteConsole(::GetStdHandle(STD_OUTPUT_HANDLE),
                                 msg,
                                 msg.GetLength(),
                                 &count,
                                 NULL);
  ASSERT1(result);
  ASSERT1(msg.GetLength() == static_cast<int>(count));
  return result;
}

void NetDiags::Initialize() {
  if (!SystemInfo::IsRunningOnXPOrLater()) {
    ::MessageBox(NULL,
        _T("GoogleUpdate.exe"),
        _T("\"GoogleUpdate.exe /NetDiags\" only runs on Windows XP or later."),
        MB_OK);
    ::ExitProcess(1);
  }

  if (!AttachConsoleWrap(ATTACH_PARENT_PROCESS)) {
    ::MessageBox(NULL,
        _T("GoogleUpdate.exe"),
        _T("Please run \"GoogleUpdate.exe /NetDiags\" from a cmd.exe window."),
        MB_OK);
    ::ExitProcess(1);
  }

  PrintToConsole(_T("\n"));
  HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    PrintToConsole(_T("Failed to ::CoInitialize() [0x%x]\n"), hr);
    ::ExitProcess(1);
  }

  // Initialize the detection chain: GoogleProxy, FireFox if it is the
  // default browser, and IE.
  NetworkConfig& network_config(NetworkConfig::Instance());
  network_config.Clear();
  BrowserType browser_type(BROWSER_UNKNOWN);
  GetDefaultBrowserType(&browser_type);
  if (browser_type == BROWSER_FIREFOX) {
    PrintToConsole(_T("Default browser is Firefox\n"));
    network_config.Add(new FirefoxProxyDetector());
  }
  network_config.Add(new IEProxyDetector());

  std::vector<Config> configs = network_config.GetConfigurations();
  if (configs.empty()) {
    PrintToConsole(_T("No Network Configurations to display\n"));
  } else {
    PrintToConsole(_T("[Detected Network Configurations][\n%s]\n"),
                   NetworkConfig::ToString(configs));
  }
}

void NetDiags::OnProgress(int bytes, int bytes_total, int, const TCHAR*) {
  PrintToConsole(_T("\n[Downloading %d of %d]\n"), bytes, bytes_total);
}

// http get.
void NetDiags::DoGet(const CString& url) {
  PrintToConsole(_T("\nGET request for [%s]\n"), url);
  NetworkRequest network_request(NetworkConfig::Instance().session());
  network_request.set_callback(this);
  network_request.set_num_retries(2);
  CString response;
  HRESULT hr = GetRequest(&network_request, url, &response);
  int status = network_request.http_status_code();
  if (FAILED(hr)) {
    PrintToConsole(_T("GET request failed. HRESULT=[0x%x], HTTP Status=[%d]\n"),
                   hr, status);
    return;
  }

  PrintToConsole(_T("HTTP Status=[%d]\n"), status);
  PrintToConsole(_T("HTTP Response=\n[%s]\n"), response);
}

// http download.
void NetDiags::DoDownload(const CString& url) {
  PrintToConsole(_T("\nDownload request for [%s]\n"), url);
  NetworkRequest network_request(NetworkConfig::Instance().session());
  network_request.set_callback(this);
  network_request.set_num_retries(2);

  CString temp_dir = app_util::GetTempDir();
  CString temp_file;
  if (!::GetTempFileName(temp_dir,
                         _T("tmp"),
                         0,
                         CStrBuf(temp_file, MAX_PATH))) {
    PrintToConsole(_T("::GetTempFileName Failed [%d]\n"), ::GetLastError());
    return;
  }

  HRESULT hr = network_request.DownloadFile(url, temp_file);
  int status = network_request.http_status_code();
  if (FAILED(hr)) {
    PrintToConsole(_T("Download failed. HRESULT=[0x%x], HTTP Status=[%d]\n"),
                   hr, status);
    return;
  }

  PrintToConsole(_T("HTTP Status=[%d]\n"), status);
  PrintToConsole(_T("Downloaded File=[%s]\n"), temp_file);
  if (!::DeleteFile(temp_file)) {
    PrintToConsole(_T("::DeleteFile Failed [%s][%d]\n"),
                   temp_file, ::GetLastError());
    return;
  } else {
    PrintToConsole(_T("Deleted file [%s]\n"), temp_file);
  }
}

// Run the tests.
int NetDiags::Main() {
  DoGet(_T("http://www.google.com/robots.txt"));
  DoGet(_T("https://www.google.com/robots.txt"));
  DoDownload(_T("http://www.google.com/intl/en_ALL/images/logo.gif"));
  return 0;
}

}  // namespace omaha

