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
#include "omaha/base/synchronized.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class NetworkRequest;
class UpdateResponse;
struct NamedObjectAttributes;

// The enum for the values of the |start_mode| parameter for the function
// StartGoogleUpdateWithArgs().
enum class StartMode { kForeground, kBackground };

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

// Builds the path of the crash handler. The command line is of the form
// "<install location>\googlecrashhandler.exe"; googlecrashhandler64.exe is
// used if use64bit is true.
CString BuildGoogleUpdateServicesPath(bool is_machine, bool use64bit);

// Builds the path of the crash handler and adds the enclosing quotes.
CString BuildGoogleUpdateServicesEnclosedPath(bool is_machine, bool use64bit);

// Returns true if the currently executing binary is running from the
// Machine/User Goopdate directory, or a directory under it.
bool IsRunningFromOfficialGoopdateDir(bool is_machine);

// Returns true if the current process is running under the passed-in csidl,
// or a directory under it.
bool IsRunningFromDir(int csidl);

// If running the installed machine instance, returns HKLM. Else returns HKCU.
CString GetHKRoot();

// Returns the version of GoogleUpdate.exe that is installed in the official
// location. Returns an empty CString if GoogleUpdate.exe is missing.
CString GetInstalledShellVersion(bool is_machine);

// Starts an instance of the Google Update version found in the registry.
// args can be NULL.
// process can be NULL. If not NULL, caller is responsible for closing handle.
HRESULT StartGoogleUpdateWithArgs(bool is_machine,
                                  StartMode start_mode,
                                  const TCHAR* args,
                                  HANDLE* process);

// Starts an instance of GoogleCrashHandler.exe, and GoogleCrashHandler64.exe
// if we're running on a 64-bit OS.
HRESULT StartCrashHandler(bool is_machine);

// Starts the metainstaller in the same directory as the current module in an
// elevated mode using the "Runas" verb.
HRESULT StartElevatedMetainstaller(const TCHAR* args, DWORD* exit_code);

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

// Obtains the OS version as a dotted-quad version and the service pack string.
HRESULT GetOSInfo(CString* os_version, CString* service_pack);

// Returns the install directory for the specified version.
CPath BuildInstallDirectory(bool is_machine, const CString& version);

// Launches the command line. On Vista and later, for a machine install, this
// method will launch the process at medium/low integrity, by impersonating the
// medium integrity token of the active user. If |process| is not null, it will
// receive a handle to the launched process which the caller is responsible for
// closing. If |child_stdout| is not null, it will receive a handle to the
// stdout stream of the launched process which the caller is responsible for
// closing.
HRESULT LaunchCmdLine(bool is_machine,
                      const CString& cmd_line,
                      HANDLE* process,
                      HANDLE* child_stdout);

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

HRESULT WriteNameValuePairsToHandle(const HANDLE file_handle,
                                    const CString& group_name,
                                    const std::map<CString, CString>& pairs);

// Returns true is any of the install workers is running.
bool IsAppInstallWorkerRunning(bool is_machine);

// Converts the installer_data value to UTF8. Then writes this UTF8 data
// prefixed with the UTF8 BOM of EF BB BF to a temp file in `directory`. Returns
// the path to temp file that was created.  The returned path will be
// quote-enclosed by EnclosePath().
HRESULT WriteInstallerDataToTempFile(const CPath& directory,
                                     const CString& installer_data,
                                     CString* installer_data_file_path);

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

// Get base64 hashed physical MAC addresses on the machine.
HRESULT GetMacHashesViaNDIS(std::vector<CString>* mac_hashes);

// Common lock for all UID functions.
HRESULT InitializeUserIdLock(bool is_machine, GLock* user_id_lock);

// These functions allow for resetting the UserID if there is no match with
// previously seen MAC addresses.
HRESULT ResetMacHashesInRegistry(bool is_machine,
                                 std::vector<CString> mac_hashes);
HRESULT ResetUserIdIfMacMismatch(bool is_machine);
HRESULT ResetUserId(bool is_machine, bool is_legacy);

// Creates a user unique id and saves it in registry if the machine is not in
// the OEM install mode.
HRESULT CreateUserId(bool is_machine);

// Deletes the user id from registry.
void DeleteUserId(bool is_machine);

// Lazy creates (if necessary) and returns the user ID in registry if the
// machine is NOT in OEM install state and current user opts in usage stats.
// Otherwise deletes the user ID from the registry and returns empty string.
CString GetUserIdLazyInit(bool is_machine);

// Returns a string that contains the first user id ever created on the system,
// along with certain feature strings appened, such as "legacy" for legacy
// old user id, "age=<days since last uid creation>", or
// "cnt=<num of uid rotations>". The attributes are separated by "; "
CString GetUserIdHistory(bool is_machine);

// Creates a global event to prevent collision between Omaha and a product's
// internal updater.  If the event doesn't already exist, it creates the event
// and returns the handle + S_OK; if the object already exists, *event_handle
// receives NULL and it returns GOOPDATE_E_APP_USING_EXTERNAL_UPDATER.
//
// Note that neither Omaha nor external updaters are expected to care about the
// signaled/unsignaled state of the event, or its ACL.  All it cares about is
// that an object with that name exists in the global object namespace.  Events
// were chosen over a lock file / registry entry because they are guaranteed to
// be cleaned up if Omaha (or the external updater) crashes.
HRESULT CreateExternalUpdaterActiveEvent(const CString& app_id,
                                         bool is_machine,
                                         scoped_event* event);

}  // namespace goopdate_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_GOOPDATE_UTILS_H_
