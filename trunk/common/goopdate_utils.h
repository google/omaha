// Copyright 2007-2010 Google Inc.
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

#ifndef OMAHA_COMMON_GOOPDATE_UTILS_H_
#define OMAHA_COMMON_GOOPDATE_UTILS_H_

#include <windows.h>
#include <atlpath.h>
#include <atlstr.h>
#include <map>
#include <vector>
// TODO(omaha3): Move all browser related functions into browser_utils or some
// similar file so we can avoid including browser_utils.h in this header. This
// is especially important because of the duplicate BrowserType definition.
#include "omaha/base/browser_utils.h"

namespace omaha {

class NetworkRequest;
class UpdateResponse;
struct NamedObjectAttributes;

// Represents the Result of an attempt to terminate the browser.
struct TerminateBrowserResult {
  TerminateBrowserResult()
      : found(false),
        could_terminate(false) {
  }

  TerminateBrowserResult(bool f, bool terminate)
      : found(f),
        could_terminate(terminate) {
  }

  bool found;
  bool could_terminate;
};

namespace goopdate_utils {

typedef HRESULT (*RegisterOrUnregisterFunction)(void* data, bool is_register);

// Builds the directory of the Google Update executable.
CString BuildGoogleUpdateExeDir(bool is_machine);

// Builds the path of the Google Update version found in the registry. The
// command line is of the form "<install location>\googleupdate.exe"
CString BuildGoogleUpdateExePath(bool is_machine);

CString BuildGoogleUpdateServicesPath(bool is_machine);

// Returns true if the currently executing binary is running from the
// Machine/User Goopdate directory, or a directory under it.
bool IsRunningFromOfficialGoopdateDir(bool is_machine);

// If running the installed machine instance, returns HKLM. Else returns HKCU.
CString GetHKRoot();

// Starts an instance of the Google Update version found in the registry.
// Only use to start interactive processes because it uses ::ShellExecuteEx().
// args can be NULL.
// process can be NULL. If not NULL, caller is responsible for closing handle.
HRESULT StartGoogleUpdateWithArgs(bool is_machine,
                                  const TCHAR* args,
                                  HANDLE* process);

// Starts an instance of GoogleCrashHandler.exe.
HRESULT StartCrashHandler(bool is_machine);

// Starts self in an elevated mode using the "Runas" verb.
HRESULT StartElevatedSelfWithArgsAndWait(const TCHAR* args, DWORD* exit_code);

// Registers security and sets the security values for the GoogleUpdate
// process when running as a COM server.
HRESULT InitializeSecurity();

// GetProductName is temporary and must be removed after the TT release.
// Gets the product name for a app guid.
CString GetProductName(const CString& app_guid);

// Returns true if it is a development or test machine.
bool IsTestSource();

HRESULT RedirectHKCR(bool is_machine);

HRESULT RemoveRedirectHKCR();

HRESULT RegisterTypeLib(bool is_admin,
                        const CComBSTR& path,
                        ITypeLib* type_lib);
HRESULT UnRegisterTypeLib(bool is_admin,
                          const CComBSTR&,
                          ITypeLib* type_lib);

HRESULT RegisterOrUnregisterModule(bool is_machine,
                                   bool register_server,
                                   RegisterOrUnregisterFunction registrar,
                                   void* data);

HRESULT RegisterOrUnregisterModuleWithTypelib(
    bool is_machine,
    bool register_server,
    RegisterOrUnregisterFunction registrar,
    void* data);

// Registers the typelib that is passed in.
// Wrapper for the RegisterTypeLibForUser that is defined in the
// Vista oleaut32. Uses GetProcAddress to call into the method.
HRESULT RegisterTypeLibForUser(ITypeLib* lib,
                               OLECHAR* path,
                               OLECHAR* help_dir);

// Unregisters the typelib that is passed in.
// Wrapper for the UnRegisterTypeLibForUser in Vista ole. Uses GetProcAddress
// to call into the real method.
HRESULT UnRegisterTypeLibForUser(REFGUID lib_id,
                                 WORD major_ver_num,
                                 WORD minor_ver_num,
                                 LCID lcid,
                                 SYSKIND syskind);

CString GetCurrentVersionedName(bool is_machine,
                                const TCHAR* value_name,
                                const TCHAR* default_val);

HRESULT CreateAndSetVersionedNameInRegistry(bool is_machine,
                                            const TCHAR* prefix,
                                            const TCHAR* value_name);

// Returns the absolute path of the browser image.
HRESULT GetBrowserImagePathFromProcess(BrowserType type,
                                       uint32 explorer_pid,
                                       CString* path);

// Terminates all browser processes for the current user.
HRESULT TerminateBrowserProcesses(BrowserType type,
                                  TerminateBrowserResult* browser_res,
                                  TerminateBrowserResult* default_res);

// Terminates instances of all known browsers. Currently, the known browsers are
// Firefox, IE and Chrome.
HRESULT TerminateAllBrowsers(BrowserType type,
                             TerminateBrowserResult* browser_res,
                             TerminateBrowserResult* default_res);


// Converts from string to the BrowserType enum.
HRESULT ConvertStringToBrowserType(const CString& text, BrowserType* type);

// Converts from BrowserType to string.
CString ConvertBrowserTypeToString(BrowserType type);

// Returns the browser to restart.
bool GetBrowserToRestart(BrowserType type,
                         BrowserType default_type,
                         const TerminateBrowserResult& res,
                         const TerminateBrowserResult& def_res,
                         BrowserType* browser_type);

// Obtains the OS version and service pack.
HRESULT GetOSInfo(CString* os_version, CString* service_pack);

// Returns the install directory for the specified version.
CPath BuildInstallDirectory(bool is_machine, const CString& version);

// Launches the command line. On Vista and later, for a machine install, this
// method will launch the process at medium/low integrity, by impersonating the
// medium integrity token of the active user.
HRESULT LaunchCmdLine(bool is_machine, const CString& cmd_line);

// Launches the browser. On Vista and later, for a machine install, this method
// will launch the browser at medium/low integrity, by impersonating the medium
// integrity token of the active user.
HRESULT LaunchBrowser(bool is_machine, BrowserType type, const CString& url);

// Gets a list of install worker processes relevant to user/machine instances.
HRESULT GetInstallWorkerProcesses(bool is_machine,
                                  std::vector<uint32>* processes);

// Creates a unique event name and stores it in the specified environment var.
HRESULT CreateUniqueEventInEnvironment(const CString& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event);

// Obtains a unique event name from specified environment var and opens it.
HRESULT OpenUniqueEventFromEnvironment(const CString& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event);

// Creates an event based on the provided attributes.
HRESULT CreateEvent(NamedObjectAttributes* event_attr, HANDLE* event_handle);

HRESULT ReadNameValuePairsFromFile(const CString& file_path,
                                   const CString& group_name,
                                   std::map<CString, CString>* pairs);

HRESULT WriteNameValuePairsToFile(const CString& file_path,
                                  const CString& group_name,
                                  const std::map<CString, CString>& pairs);

// Returns true is any of the install workers is running.
bool IsAppInstallWorkerRunning(bool is_machine);

// Returns whether the version is an "Omaha 2" version or later.
bool IsGoogleUpdate2OrLater(const CString& version);

// Converts the installer_data value to UTF8. Then writes this UTF8 data
// prefixed with the UTF8 BOM of EF BB BF to a temp file. Returns the path to
// temp file that was created.  The returned path will be quote-enclosed by
// EnclosePath().
HRESULT WriteInstallerDataToTempFile(const CString& installer_data,
                                     CString* installer_data_file_path);

// TODO(omaha): Move these two to ua_internal.h.
// Returns true if a server update check is due.
bool ShouldCheckForUpdates(bool is_machine);

// Updates LastChecked to now. Call after successful update check for all apps.
HRESULT UpdateLastChecked(bool is_machine);

// Launches the /uninstall process.
HRESULT LaunchUninstallProcess(bool is_machine);

// Returns a token that can be used to impersonate in the case of a
// machine process. The caller has ownership of the token that is returned and
// it must close the handle. The token corresponds to the primary token for
// the current or one of the logged on users but only if the caller is a
// machine process running as local system and not impersonated.
// This is a very specialized function,intended to be called by local system
// processes making network calls where the caller is not impersonated.
HANDLE GetImpersonationTokenForMachineProcess(bool is_machine);

// Enables or disables Structured Exception Handler Overwrite Protection a.k.a
// SEHOP for machine Omaha. More information on SEHOP: http://goo.gl/1hfD.
HRESULT EnableSEHOP(bool enable);

// Creates a user unique id and saves it in registry if the machine is not in
// the OEM install mode.
HRESULT CreateUserId(bool is_machine);

// Deletes the user id from registry.
void DeleteUserId(bool is_machine);

// Lazy creates (if necessary) and returns the user ID in registry if the
// machine is NOT in OEM install state and current user opts in usage stats.
// Otherwise deletes the user ID from the registry and returns empty string.
CString GetUserIdLazyInit(bool is_machine);

}  // namespace goopdate_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_GOOPDATE_UTILS_H_
