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
//
// Defines: GoopdateUtils, helper functions for goopdate.
//
// TODO(omaha): move to namespace.

#ifndef OMAHA_GOOPDATE_GOOPDATE_UTILS_H__
#define OMAHA_GOOPDATE_GOOPDATE_UTILS_H__

#include <mstask.h>
#include <oaidl.h>
#include <atlcomcli.h>
#include <atlpath.h>
#include <atlstr.h>
#include <map>
#include <vector>
#include "omaha/common/constants.h"
#include "omaha/common/shell.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/request.h"
#include "omaha/goopdate/update_response.h"

struct NamedObjectAttributes;

namespace omaha {

class UpdateResponse;
class NetworkRequest;

// TODO(omaha): Move all browser related functions into browser_utils.
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

// Utility functions for goopdate.
namespace goopdate_utils {

typedef HRESULT (*RegisterOrUnregisterFunction)(bool is_register);

// Returns the application registration path for the specified app.
CString GetAppClientsKey(bool is_machine, const CString& app_guid);

// Returns the application state path for the specified app.
CString GetAppClientStateKey(bool is_machine, const CString& app_guid);

// Returns the medium integrity application state path for the specified app.
CString GetAppClientStateMediumKey(bool is_machine, const CString& app_guid);

// Returns the application state path for a user given the user SID.
CString GetUserAllAppsStatePath(const CString& user_sid);

// Returns the application registration location given the user SID.
CString GetUserAllAppsRegPath(const CString& user_sid);

// Returns the application state path for a particular user and for the
// given application id.
CString GetUserAppStatePath(const CString& user_sid,
                            const CString& app_guid);

// Returns the application registration path for a particular user
// and for the given application id.
CString GetUserAppRegPath(const CString& user_sid,
                          const CString& app_guid);

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

// Starts self in an elevated mode using the "Runas" verb.
HRESULT StartElevatedSelfWithArgsAndWait(const TCHAR* args);

// Registers security and sets the security values for the GoogleUpdate
// process when running as a COM server.
HRESULT InitializeSecurity();

// GetProductName is temporary and must be removed after the TT release.
// Gets the product name for a app guid.
CString GetProductName(const CString& app_guid);

// Returns true if it is a development or test machine.
bool IsTestSource();

// Creates the query string based on the passed url and arguments.
// url must end in a '?' or '&'.
HRESULT BuildHttpGetString(const CString& url,
                           DWORD error_code,
                           DWORD extra_code1,
                           DWORD extra_code2,
                           const CString& product_guid,
                           const CString& goopdate_version,
                           bool is_machine,
                           const CString& language,
                           const GUID& iid,
                           const CString& brand_code,
                           const CString& source_id,
                           CString* get_request);

HRESULT RedirectHKCR(bool is_machine);

HRESULT RemoveRedirectHKCR();

HRESULT RegisterTypeLib(bool is_admin,
                        const CComBSTR& path,
                        ITypeLib* type_lib);
HRESULT UnRegisterTypeLib(bool is_admin,
                          const CComBSTR&,
                          ITypeLib* type_lib);

HRESULT RegisterOrUnregisterModule(bool register_server,
                                   RegisterOrUnregisterFunction registrar);

HRESULT RegisterOrUnregisterModuleWithTypelib(
    bool register_server,
    RegisterOrUnregisterFunction registrar);

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

// Impersonates the user in case of interactive installs, and
// the logged in user in case of non-interactive updates.
HRESULT ImpersonateUser(bool is_interactive, uint32 explorer_pid);

// Reverts the calling thread to self if impersonated.
HRESULT UndoImpersonation(bool impersonated);

// Returns the user's token either from the request or from the logged on
// user's explorer process.
HRESULT GetImpersonationToken(bool is_interactive,
                              uint32 explorer_pid,
                              HANDLE* out_token);

// Returns whether the EULA is accepted for the app.
bool IsAppEulaAccepted(bool is_machine,
                       const CString& app_guid,
                       bool require_explicit_acceptance);

// Sets eulaaccepted=0 in the app's ClientState.
HRESULT SetAppEulaNotAccepted(bool is_machine, const CString& app_guid);

// Clears any eulaaccepted=0 values for the app.
HRESULT ClearAppEulaNotAccepted(bool is_machine, const CString& app_guid);

// Determines whether usage stats are enabled for a specific app.
bool AreAppUsageStatsEnabled(bool is_machine, const CString& app_guid);

// Configures Omaha's collection of usage stats and crash reports.
HRESULT SetUsageStatsEnable(bool is_machine,
                            const CString& app_guid,
                            Tristate usage_stats_enable);

// Copies legacy usage stats value to the new name and location then deletes
// the old value.
// TODO(omaha): Remove along with legacy Omaha 1.
HRESULT ConvertLegacyUsageStats(bool is_machine);

//
// Scheduled Task methods.

// This method will return the default scheduled task name. This default value
// is also used as the prefix for generating unique task names.
CString GetDefaultGoopdateTaskName(bool is_machine, CommandLineMode mode);
HRESULT InstallGoopdateTasks(const TCHAR* task_path, bool is_machine);
HRESULT UninstallGoopdateTasks(bool is_machine);
HRESULT UninstallLegacyGoopdateTasks(bool is_machine);
HRESULT StartGoopdateTaskCore(bool is_machine);
bool IsInstalledGoopdateTaskUA(bool is_machine);
bool IsDisabledGoopdateTaskUA(bool is_machine);
HRESULT GetExitCodeGoopdateTaskUA(bool is_machine);

// Gets the specified value from the registry for the application.
HRESULT GetClientsStringValueFromRegistry(bool is_machine,
                                          const CString& app_guid,
                                          const CString& value_name,
                                          CString* value);

// Gets the application's version from the registry.
HRESULT GetVerFromRegistry(bool is_machine,
                           const CString& app_guid,
                           CString* version);

// Returns the absolute path of the browser image.
HRESULT GetBrowserImagePathFromProcess(BrowserType type,
                                       uint32 explorer_pid,
                                       CString* path);

// Terminates all browser processes for the current user.
HRESULT TerminateBrowserProcesses(BrowserType type,
                                  TerminateBrowserResult* browser_res,
                                  TerminateBrowserResult* default_res);

// Terminates both firefox and IE instances.
HRESULT TerminateAllBrowsers(BrowserType type,
                             TerminateBrowserResult* browser_res,
                             TerminateBrowserResult* default_res);


// Starts an instance of the browser. The explorer_pid indicates the user to
// launch the browser as, in case of machine goopdate.
HRESULT StartBrowserWithProcessToken(bool is_machine,
                                     BrowserType type,
                                     const CString& url,
                                     uint32 explorer_pid);

// Converts from string to the BrowserType enum.
HRESULT ConvertStringToBrowserType(const CString& text, BrowserType* type);

// Converts from BrowserType to string.
CString ConvertBrowserTypeToString(BrowserType type);

// Converts a string to the needs admin enum. Takes care of case.
HRESULT ConvertStringToNeedsAdmin(const CString& text, NeedsAdmin* admin);

// Converts the NeedsAdmin to a string.
HRESULT ConvertNeedsAdminToString(NeedsAdmin needs_admin, CString* text);

// Returns the browser to restart.
bool GetBrowserToRestart(BrowserType type,
                         BrowserType default_type,
                         const TerminateBrowserResult& res,
                         const TerminateBrowserResult& def_res,
                         BrowserType* browser_type);

// Returns whether the Goopdate service is installed.
bool IsServiceInstalled();

// Reads the value of the persistent id.
HRESULT ReadPersistentId(const CString& key_name,
                         const CString& value_name,
                         CString* id);

// Obtains the OS version and service pack.
HRESULT GetOSInfo(CString* os_version, CString* service_pack);

// Returns the install directory for the specified version.
CPath BuildInstallDirectory(bool is_machine, const CString& version);

// Launches the browser. On Vista and later, for a machine install, this method
// will launch the browser at medium/low integrity, by impersonating the medium
// integrity token of the active user.
HRESULT LaunchBrowser(BrowserType type, const CString& url);

// Launches the command line. On Vista and later, for a machine install, this
// method will launch the process at medium/low integrity, by impersonating the
// medium integrity token of the active user.
HRESULT LaunchCmdLine(const CString& cmd_line);

// Converts the data contained in the response to the extra argument format.
HRESULT ConvertResponseDataToExtraArgs(const UpdateResponseData& response,
                                       CString* extra);

// Converts the manifest_filename parameter to extra args and starts
// an omaha2 install worker from the install directory.
HRESULT HandleLegacyManifestHandoff(const CString& manifest_filename,
                                    bool is_machine);

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

// Initializes the network stack and adds the proxy detectors.
HRESULT ConfigureNetwork(bool is_machine, bool is_local_system);

HRESULT ReadNameValuePairsFromFile(const CString& file_path,
                                   const CString& group_name,
                                   std::map<CString, CString>* pairs);

HRESULT WriteNameValuePairsToFile(const CString& file_path,
                                  const CString& group_name,
                                  const std::map<CString, CString>& pairs);

// Writes branding information for Google Update in the registry if it does not
// already exist. Otherwise, the information remains unchanged.
// Writes a default Omaha-specific brand code if one is not specified in args.
HRESULT SetGoogleUpdateBranding(const CString& client_state_key_path,
                                const CString& brand_code,
                                const CString& client_id);


// Writes branding information for apps in the registry if it does not
// already exist. Otherwise, the information remains unchanged.
// Writes a default Omaha-specific brand code if one is not specified in args.
HRESULT SetAppBranding(const CString& client_state_key_path,
                       const CString& brand_code,
                       const CString& client_id,
                       const CString& referral_id);

// Returns true is any of the install workers is running.
bool IsAppInstallWorkerRunning(bool is_machine);

// Returns whether a process is a machine process.
// Does not determine whether the process has the appropriate privileges.
bool IsMachineProcess(
    CommandLineMode mode,
    bool is_running_from_official_machine_directory,
    bool is_local_system,  // Whether process is running as local system.
    bool is_machine_override,  // True if machine cmd line override specified.
    Tristate needs_admin);  // needsadmin value for primary app if present.

// Returns whether the version is an "Omaha 2" version or later.
bool IsGoogleUpdate2OrLater(const CString& version);

// Formats an error message for network errors. Returns true if the error has
// a specific message. Otherwise, formats a generic network connection message
// and returns false.
bool FormatMessageForNetworkError(HRESULT error,
                                  const CString app_name,
                                  CString* msg);

// Adds details about the network_request to the event log as an error.
void AddNetworkRequestDataToEventLog(NetworkRequest* network_request,
                                     HRESULT hr);

// Converts the installer_data value to UTF8. Then writes this UTF8 data
// prefixed with the UTF8 BOM of EF BB BF to a temp file. Returns the path to
// temp file that was created.  The returned path will be quote-enclosed by
// EnclosePath().
HRESULT WriteInstallerDataToTempFile(const CString& installer_data,
                                     CString* installer_data_file_path);

// Returns the number of clients registered under the "Clients" sub key.
HRESULT GetNumClients(bool is_machine, size_t* num_clients);

// Validates the hash of the file. If the hash does not match, checks the size
// to determine whether the problem was corruption or file size.
HRESULT ValidateDownloadedFile(const CString& file_name,
                               const CString& hash,
                               uint32 size);

void DisplayErrorInMessageBox(const CString& error_text,
                              const CString& primary_app_name);

}  // namespace goopdate_utils

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_UTILS_H__
