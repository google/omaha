// Copyright 2006 Google Inc.
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

#include "omaha/base/browser_utils.h"

#include <exdisp.h>

#include <memory>

#include "base/basictypes.h"
#include "omaha/base/commands.h"
#include "omaha/base/const_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/process.h"
#include "omaha/base/proc_utils.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/shell.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/base/thread.h"
#include "omaha/base/vistautil.h"
#include "omaha/base/vista_utils.h"

namespace omaha {

namespace {

// Attempts to force instances of IE to quit gracefully using shell APIs.
HRESULT CloseIeUsingShell(const CString& sid) {
  CComPtr<IShellWindows> shell_windows;
  HRESULT hr = shell_windows.CoCreateInstance(CLSID_ShellWindows);
  if (FAILED(hr)) {
    return hr;
  }

  long num_windows = 0;  // NOLINT
  hr = shell_windows->get_Count(&num_windows);
  if (FAILED(hr)) {
    return hr;
  }

  for (int32 i = 0; i < num_windows; ++i) {
    CComVariant index(i);

    CComPtr<IDispatch> disp;
    hr = shell_windows->Item(index, &disp);
    if (FAILED(hr)) {
      UTIL_LOG(L3, (_T("[CloseIeUsingShell][Item failed][0x%08x]"), hr));
      return hr;
    }
    if (!disp) {
      // Skip - this shell window was registered with a NULL IDispatch
      continue;
    }

    CComPtr<IWebBrowser2> browser;
    hr = disp.QueryInterface(&browser);
    if (FAILED(hr)) {
      // Skip - this shell window doesn't implement IWebBrowser2
      continue;
    }

    // Okay, this window implements IWebBrowser2, so it's potentially an
    // IE session.  Identify the owning process.
    LONG_PTR hwnd = 0;
    hr = browser->get_HWND(&hwnd);
    if (FAILED(hr)) {
      UTIL_LOG(L3, (_T("[CloseIeUsingShell][get_HWND failed][0x%08x]"), hr));
      continue;
    }

    DWORD process_id = 0;
    ::GetWindowThreadProcessId(reinterpret_cast<HWND>(hwnd), &process_id);
    if (0 == process_id) {
      UTIL_LOG(L3, (_T("[CloseIeUsingShell][invalid process id]")));
      continue;
    }

    // If a SID was passed in, check that the SID owns this process.  (If we
    // can't query the process owner, play it safe and skip it.)
    if (!sid.IsEmpty()) {
      CString process_sid;
      hr = Process::GetProcessOwner(process_id, &process_sid);
      if (FAILED(hr)) {
        UTIL_LOG(L3, (_T("[CloseIeUsingShell][GetProcessOwner][0x%08x]"), hr));
        continue;
      }

      if (0 != sid.CompareNoCase(process_sid)) {
        // Skip - process is not owned by us
        continue;
      }
    }

    // Verify that the process executable is iexplore.exe, as this list may
    // also contain embedded shdocvw sessions in explorer.exe.  (If we can't
    // fetch the process executable, play it safe and skip it.)
    CString process_module;
    hr = Process::GetExecutablePath(process_id, &process_module);
    if (FAILED(hr)) {
      UTIL_LOG(L3, (_T("[CloseIeUsingShell][GetExecutablePath][0x%08x]"), hr));
      continue;
    }

    if (0 != GetFileFromPath(process_module).CompareNoCase(kIeExeName)) {
      // Skip - it's not an iexplore.exe
      continue;
    }

    // Okay, this is an IWebBrowser2 hosted by an iexplore.exe that is running
    // under the requested SID.  Ask it to quit.  (Note: It may not necessarily
    // honor this request.  We can follow it up with WM_CLOSE messages or a
    // process termination if absolutely necessary.)
    UTIL_LOG(L3, (_T("[CloseIeUsingShell][Closing IE session][pid %d]"),
             process_id));

    hr = browser->Quit();
    if (FAILED(hr)) {
      UTIL_LOG(L3, (_T("[CloseIeUsingShell][Quit failed][0x%08x]"), hr));
      continue;
    }
  }

  return S_OK;
}

class CloseIeUsingShellRunnable : public Runnable {
 public:
  explicit CloseIeUsingShellRunnable(const CString& sid) : sid_(sid) {}
 protected:
  virtual ~CloseIeUsingShellRunnable() {}

  virtual void Run() {
    UTIL_LOG(L3, (_T("[CloseIeUsingShellRunnable][%s]"), sid_));

    scoped_co_init co_init(COINIT_MULTITHREADED);
    VERIFY_SUCCEEDED(co_init.hresult());

    CloseIeUsingShell(sid_);
    delete this;
  }

 private:
  CString sid_;
};

}  // end namespace

HRESULT GetLegacyDefaultBrowserInfo(CString* name, CString* path) {
  UTIL_LOG(L3, (_T("[GetLegacyDefaultBrowserInfo]")));
  ASSERT1(name);
  ASSERT1(path);
  name->Empty();
  path->Empty();

  CString browser_command_line;
  HRESULT hr = RegKey::GetValue(kRegKeyLegacyDefaultBrowserCommand,
                                NULL,
                                &browser_command_line);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[Read failed][%s]"), kRegKeyLegacyDefaultBrowserCommand));
    return hr;
  }

  UTIL_LOG(L5, (_T("[browser_command_line][%s]"), browser_command_line));
  browser_command_line.Trim(_T(" "));
  if (browser_command_line.IsEmpty()) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  CString browser_path;
  if (File::Exists(browser_command_line)) {
    if (File::IsDirectory(browser_command_line)) {
      UTIL_LOG(LE, (_T("[Unexpected. Command line should not be directory]")));
      return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    browser_path = browser_command_line;
  } else {
    hr = GetExePathFromCommandLine(browser_command_line, &browser_path);
    if (FAILED(hr)) {
      return hr;
    }

    if (!File::Exists(browser_path) ||
        File::IsDirectory(browser_path)) {
      UTIL_LOG(LE, (_T("[browser_path invalid][%s]"), browser_path));
      return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }
  }

  CString browser_name = ::PathFindFileName(browser_path);
  if (browser_name.IsEmpty() ||
      !String_EndsWith(browser_name, _T(".exe"), true)) {
    UTIL_LOG(LE, (_T("[browser name invalid][%s]"), browser_name));
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  *path = browser_path;
  *name = browser_name;
  return S_OK;
}

HRESULT GetDefaultBrowserName(CString* name) {
  ASSERT1(name);
  name->Empty();

  // Get the default browser name from current user registry.
  HKEY user_root_key = NULL;
  VERIFY1(::RegOpenCurrentUser(KEY_READ, &user_root_key) == ERROR_SUCCESS);
  RegKey user_default_browser;
  HRESULT hr = user_default_browser.Open(
      user_root_key ? user_root_key : HKEY_CURRENT_USER,
      kRegKeyDefaultBrowser,
      KEY_READ);
  if (SUCCEEDED(hr)) {
    hr = user_default_browser.GetValue(NULL, name);
  }
  if (user_root_key) {
    LONG error(::RegCloseKey(user_root_key));

    // See bug http://b/1231862 for details when RegCloseKey can
    // return ERROR_INVALID_HANDLE.
    ASSERT1(error == ERROR_SUCCESS ||
            error == ERROR_INVALID_HANDLE);
  }

  if (SUCCEEDED(hr) && !name->IsEmpty()) {
    UTIL_LOG(L3, (_T("[Default browser for the user is %s]"), *name));
    return S_OK;
  }

  // Try to get from local machine registry.
  hr = RegKey::GetValue(kRegKeyMachineDefaultBrowser, NULL, name);
  if (SUCCEEDED(hr) && !name->IsEmpty()) {
    UTIL_LOG(L3, (_T("[Default browser for the machine is %s]"), *name));
    return S_OK;
  }

  // Try to get from legacy default browser location.
  CString browser_path;
  hr = GetLegacyDefaultBrowserInfo(name, &browser_path);
  if (SUCCEEDED(hr) && !name->IsEmpty()) {
    UTIL_LOG(L3, (_T("[Legacy default browser is %s]"), *name));
    return S_OK;
  }

  // If still failed, we don't want to break in this case so we default to
  // IEXPLORE.EXE. A scenario where this can happen is when the user installs
  // a browser that configures itself to be default. Then the user uninstalls
  // or somehow removes that browser and the registry is not updated correctly.
  *name = kIeExeName;
  return S_OK;
}

HRESULT GetDefaultBrowserType(BrowserType* type) {
  ASSERT1(type);

  CString name;
  HRESULT hr = GetDefaultBrowserName(&name);
  if (FAILED(hr)) {
    return hr;
  }

  if (name.CompareNoCase(kIeExeName) == 0) {
    *type = BROWSER_IE;
  } else if (name.CompareNoCase(kFirefoxExeName) == 0) {
    *type = BROWSER_FIREFOX;
  } else if (name.CompareNoCase(kChromeExeName) == 0 ||
             name.CompareNoCase(kChromeBrowserName) == 0) {
    *type = BROWSER_CHROME;
  } else {
    *type = BROWSER_UNKNOWN;
  }

  return S_OK;
}

// Returns a path that is always unenclosed. The method could return a short or
// long form path.
HRESULT GetDefaultBrowserPath(CString* path) {
  ASSERT1(path);
  path->Empty();
  ON_SCOPE_EXIT(UnenclosePath, path);

  // Get the default browser name.
  CString name;
  if (FAILED(GetDefaultBrowserName(&name))) {
    // Try getting IE's path through COM registration and app path entries.
    return GetBrowserImagePath(BROWSER_IE, path);
  }

  CString shell_open_path(_T("\\"));
  shell_open_path += name;
  shell_open_path += kRegKeyShellOpenCommand;

  // Read the path corresponding to it from current user registry.
  CString browser_key(kRegKeyUserDefaultBrowser);
  browser_key += shell_open_path;
  HRESULT hr = RegKey::GetValue(browser_key, NULL, path);

  CString cmd_line(*path);
  CString args;
  if (SUCCEEDED(hr) &&
      !path->IsEmpty() &&
      SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(cmd_line,
                                                           path,
                                                           &args))) {
    return S_OK;
  }

  // If failed, try to get from local machine registry.
  browser_key = kRegKeyMachineDefaultBrowser;
  browser_key += shell_open_path;
  hr = RegKey::GetValue(browser_key, NULL, path);

  cmd_line = *path;
  args.Empty();
  if (SUCCEEDED(hr) &&
      !path->IsEmpty() &&
      SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(cmd_line,
                                                           path,
                                                           &args))) {
    return S_OK;
  }

  // Try to get from legacy default browser location.
  hr = GetLegacyDefaultBrowserInfo(&name, path);
  if (SUCCEEDED(hr) && !path->IsEmpty()) {
    return S_OK;
  }

  // If failed and the default browser is not IE, try IE once again.
  if (name.CompareNoCase(kIeExeName) != 0) {
    browser_key = kRegKeyMachineDefaultBrowser
                  _T("\\") kIeExeName kRegKeyShellOpenCommand;
    hr = RegKey::GetValue(browser_key, NULL, path);

    cmd_line = *path;
    args.Empty();
    if (SUCCEEDED(hr) &&
        !path->IsEmpty() &&
        SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(cmd_line,
                                                             path,
                                                             &args))) {
      return S_OK;
    }
  }

  // Try getting the default browser's path through COM registration and app
  // path entries.
  BrowserType default_type = BROWSER_UNKNOWN;
  hr = GetDefaultBrowserType(&default_type);
  if (FAILED(hr) ||
      default_type == BROWSER_UNKNOWN ||
      default_type == BROWSER_DEFAULT) {
    default_type = BROWSER_IE;
  }

  return GetBrowserImagePath(default_type, path);
}

HRESULT GetIEPath(CString* path) {
  ASSERT1(path);

  CString ie_command_line;
  HRESULT hr = RegKey::GetValue(kRegKeyIeClass,
                                kRegValueIeClass,
                                &ie_command_line);
  if (FAILED(hr)) {
    return hr;
  }

  if (ie_command_line.IsEmpty()) {
    return E_UNEXPECTED;
  }

  CString ie_path;
  CString args;
  hr = CommandParsingSimple::SplitExeAndArgsGuess(ie_command_line,
                                                  &ie_path,
                                                  &args);
  if (FAILED(hr)) {
    return hr;
  }

  UnenclosePath(&ie_path);

  if (!String_EndsWith(ie_path, kIeExeName, true)) {
    // On Win8 64-bit the Wow6432 node has:
    //   "C:\Program Files (x86)\Internet Explorer\ielowutil.exe" -CLSID:
    //   {0002DF01-0000-0000-C000-000000000046
    // ielowutil.exe will not launch the browser. So we need to use iexplore.exe
    // from the same directory containing ielowutil.exe.
    ie_path = ConcatenatePath(GetDirectoryFromPath(ie_path), kIeExeName);
  }

  EnclosePath(&ie_path);
  *path = ie_path;
  return S_OK;
}

HRESULT BrowserTypeToProcessName(BrowserType type, CString* exe_name) {
  ASSERT1(exe_name);
  ASSERT1(type < BROWSER_MAX);

  switch (type) {
    case BROWSER_IE:
      *exe_name = kIeExeName;
      break;
    case BROWSER_FIREFOX:
      *exe_name = kFirefoxExeName;
      break;
    case BROWSER_CHROME:
      *exe_name = kChromeExeName;
      break;
    case BROWSER_DEFAULT:
      return GetDefaultBrowserName(exe_name);
    case BROWSER_UNKNOWN:
      // Fall through.
    case BROWSER_MAX:
      // Fall through.
    default:
      return E_FAIL;
  }

  return S_OK;
}

// Returns a path that is always unenclosed. The method could return a short or
// long form path.
HRESULT GetBrowserImagePath(BrowserType type, CString* path) {
  ASSERT1(path);
  ASSERT1(type < BROWSER_MAX);

  HRESULT hr = E_FAIL;
  switch (type) {
    case BROWSER_IE: {
      hr = GetIEPath(path);
      break;
    }
    case BROWSER_FIREFOX: {
      hr = RegKey::GetValue(kRegKeyFirefox, kRegValueFirefox, path);
      if (SUCCEEDED(hr) && !path->IsEmpty()) {
        // The Firefox registry key contains a -url %1 value. Remove this
        // because we only want to return the path.
        ReplaceCString(*path, _T("-url \"%1\""), _T(""));
      }
      break;
    }
    case BROWSER_CHROME: {
      hr = RegKey::GetValue(kRegKeyChrome, kRegValueChrome, path);
      if (SUCCEEDED(hr) && !path->IsEmpty()) {
        // The Chrome registry key contains a -- "%1" value. Remove this because
        // we only want to return the path.
        ReplaceCString(*path, _T("-- \"%1\""), _T(""));
      }
      break;
    }
    case BROWSER_DEFAULT: {
      hr = GetDefaultBrowserPath(path);
      if (FAILED(hr)) {
        UTIL_LOG(LE, (_T("[GetDefaultBrowserPath failed.][0x%08x]"), hr));
      }
      path->Trim();
      UnenclosePath(path);
      return hr;
    }
    case BROWSER_UNKNOWN:
      // Fall through.
    case BROWSER_MAX:
      // Fall through.
    default:
      return E_FAIL;
  }

  CString cmd_line(*path);
  CString args;
  if (SUCCEEDED(hr) &&
      !path->IsEmpty() &&
      SUCCEEDED(CommandParsingSimple::SplitExeAndArgsGuess(cmd_line,
                                                           path,
                                                           &args))) {
    return S_OK;
  }

  // If the above did not work, then we try to read the value from
  // "HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths".
  CString browser_exe_name;
  hr = BrowserTypeToProcessName(type, &browser_exe_name);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[BrowserTypeToProcessName failed][0x%08x]"), hr));
    return hr;
  }

  CString exe_path;
  hr = Shell::GetApplicationExecutablePath(browser_exe_name, &exe_path);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetApplicationExecutablePath failed][0x%08x]"), hr));
    return hr;
  }

  *path = ConcatenatePath(exe_path, browser_exe_name);
  ASSERT1(!path->IsEmpty());

  path->Trim();
  UnenclosePath(path);
  return S_OK;
}

HRESULT TerminateBrowserProcess(BrowserType type,
                                const CString& sid,
                                int timeout_msec,
                                bool* found) {
  UTIL_LOG(L3, (_T("[TerminateBrowserProcess][%d]"), type));
  ASSERT1(found);
  ASSERT1(type < BROWSER_MAX);

  *found = false;
  if (type == BROWSER_UNKNOWN || type >= BROWSER_MAX) {
    return E_FAIL;
  }

  if (type == BROWSER_IE) {
    Thread close_ie_thread;

    close_ie_thread.Start(new CloseIeUsingShellRunnable(sid));
    close_ie_thread.WaitTillExit(timeout_msec);

    // Fall through after attempting to close IE via automation - if it
    // succeeded, we'll find no processes, but if it failed we still want
    // to attempt a conventional WM_CLOSE quit.  (This also applies to
    // third-party hosts of IE that may not respect IWebBrowser2::Quit.)
  }

  CString browser_exe_name;
  HRESULT hr = BrowserTypeToProcessName(type, &browser_exe_name);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[BrowserTypeToProcessName failed][0x%08x]"), hr));
    return hr;
  }

  DWORD current_session = System::GetCurrentSessionId();
  uint32 method_mask = ProcessTerminator::KILL_METHOD_1_WINDOW_MESSAGE |
                       ProcessTerminator::KILL_METHOD_2_THREAD_MESSAGE |
                       ProcessTerminator::KILL_METHOD_4_TERMINATE_PROCESS;
  ProcessTerminator process(browser_exe_name, sid, current_session);
  hr = process.KillTheProcess(timeout_msec, found, method_mask, true);
  if (FAILED(hr)) {
    UTIL_LOG(LEVEL_WARNING, (_T("[KillTheProcess failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT WaitForBrowserToDie(BrowserType type,
                            const CString& sid,
                            int timeout_msec) {
  UTIL_LOG(L3, (_T("[WaitForBrowserToDie][%d]"), type));
  ASSERT1(type < BROWSER_MAX);

  if (type == BROWSER_UNKNOWN || type >= BROWSER_MAX) {
    return E_FAIL;
  }

  CString browser_exe_name;
  HRESULT hr = BrowserTypeToProcessName(type, &browser_exe_name);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[BrowserTypeToProcessName failed][0x%08x]"), hr));
    return hr;
  }

  DWORD current_session = System::GetCurrentSessionId();
  ProcessTerminator process(browser_exe_name, sid, current_session);
  hr = process.WaitForAllToDie(timeout_msec);
  if (FAILED(hr)) {
    UTIL_LOG(LEVEL_WARNING, (_T("[WaitForAllToDie failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT RunBrowser(BrowserType type, const CString& url) {
  UTIL_LOG(L3, (_T("[RunBrowser][%d][%s]"), type, url));
  ASSERT1(type < BROWSER_MAX);

  if (type == BROWSER_UNKNOWN || type >= BROWSER_MAX) {
    return E_FAIL;
  }

  CString path;
  HRESULT hr =  GetBrowserImagePath(type, &path);
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[GetBrowserImagePath failed][0x%08x]"), hr));
    // ShellExecute the url directly as a last resort.
    return Shell::Execute(url);
  }
  EnclosePath(&path);

  UTIL_LOG(L3, (_T("[Execute browser][%s][%s]"), path, url));

  // http://b/1219313: For Vista, in some cases, using ShellExecuteEx does not
  // re-launch the process. So, using CreateProcess instead.
  // http://b/1223658: For Vista, especially in the impersonated case, need to
  // create a fresh environment block. Otherwise, the environment is tainted.
  hr = vista::RunAsCurrentUser(path + _T(' ') + url, NULL, NULL);

  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[RunAsCurrentUser failed][0x%x]"), hr));
    // ShellExecute the url directly as a last resort.
    return Shell::Execute(url);
  }

  return S_OK;
}

// Gets the font size of IE. This is the value that corresponds to what IE
// displays in "Page/Text Size" menu option. There are 5 values and the default
// is "Medium", for which the numeric value is 2. The "IEFontSize" is only
// present after the user has modified the default text size in IE, therefore
// the absence of the value indicates "Medium" text size.
HRESULT GetIeFontSize(uint32* font_size) {
  ASSERT1(font_size);

  const TCHAR ie_scripts_key[] =
    _T("HKCU\\Software\\Microsoft\\Internet Explorer\\International\\Scripts\\3");  // NOLINT
  const TCHAR ie_font_size[] = _T("IEFontSize");
  const uint32 kDefaultFontSize = 2;

  // We expect the scripts key to be there in all cases. The "IEFontSize" value
  // is optional but we want to fail if the key is not there.
  if (!RegKey::HasKey(ie_scripts_key)) {
    return E_UNEXPECTED;
  }

  std::unique_ptr<byte[]> buf;   // The font size is a binary registry value.
  size_t buf_size(0);
  if (FAILED(RegKey::GetValue(ie_scripts_key, ie_font_size,
                              &buf, &buf_size))) {
    *font_size = kDefaultFontSize;
    return S_OK;
  }

  ASSERT1(buf_size == sizeof(uint32));  // NOLINT
  if (buf_size != sizeof(uint32)) {     // NOLINT
    return E_UNEXPECTED;
  }

  uint32 val = *reinterpret_cast<uint32*>(buf.get());
  ASSERT1(val <= 4);
  if (val > 4) {
    return E_UNEXPECTED;
  }

  *font_size = val;
  return S_OK;
}

}  // namespace omaha
