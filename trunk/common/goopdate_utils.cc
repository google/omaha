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

#include "omaha/common/goopdate_utils.h"
#include <atlsecurity.h>
#include "omaha/base/app_util.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/const_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/proc_utils.h"
#include "omaha/base/process.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/service_utils.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/system_info.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/oem_install_utils.h"
#include "omaha/statsreport/metrics.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

namespace goopdate_utils {

namespace {

const int kTerminateBrowserTimeoutMs = 60000;

bool IsMachineProcessWithoutPrivileges(bool is_machine_process) {
  return is_machine_process && !vista_util::IsUserAdmin();
}

HRESULT LaunchImpersonatedCmdLine(const CString& cmd_line) {
  CORE_LOG(L3, (_T("[LaunchImpersonatedCmdLine][%s]"), cmd_line));

  scoped_handle impersonation_token;
  HRESULT hr = vista::GetLoggedOnUserToken(address(impersonation_token));
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetLoggedOnUserToken failed][0x%x]"), hr));
    return hr;
  }

  scoped_impersonation impersonate_user(get(impersonation_token));
  hr = HRESULT_FROM_WIN32(impersonate_user.result());
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[impersonation failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IProcessLauncher> launcher;
  hr = launcher.CoCreateInstance(CLSID_ProcessLauncherClass,
                                 NULL,
                                 CLSCTX_LOCAL_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[CoCreateInstance IProcessLauncher failed][0x%x]"), hr));
    return hr;
  }

  hr = launcher->LaunchCmdLine(cmd_line);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IProcessLauncher.LaunchBrowser failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT LaunchImpersonatedBrowser(BrowserType type, const CString& url) {
  CORE_LOG(L3, (_T("[LaunchImpersonatedBrowser][%u][%s]"), type, url));

  scoped_handle impersonation_token;
  HRESULT hr = vista::GetLoggedOnUserToken(address(impersonation_token));
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetLoggedOnUserToken failed][0x%x]"), hr));
    return hr;
  }

  scoped_impersonation impersonate_user(get(impersonation_token));
  hr = HRESULT_FROM_WIN32(impersonate_user.result());
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[impersonation failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IProcessLauncher> launcher;
  hr = launcher.CoCreateInstance(CLSID_ProcessLauncherClass,
                                 NULL,
                                 CLSCTX_LOCAL_SERVER);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[CoCreateInstance IProcessLauncher failed][0x%x]"), hr));
    return hr;
  }

  hr = launcher->LaunchBrowser(type, url);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[IProcessLauncher.LaunchBrowser failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

}  // namespace

HRESULT LaunchCmdLine(bool is_machine, const CString& cmd_line) {
  CORE_LOG(L3, (_T("[LaunchCmdLine][%d][%s]"), is_machine, cmd_line));

  if (is_machine && vista_util::IsVistaOrLater() && vista_util::IsUserAdmin()) {
    return LaunchImpersonatedCmdLine(cmd_line);
  }

  HRESULT hr = System::ShellExecuteCommandLine(cmd_line, NULL, NULL);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[ShellExecuteCommandLine failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT LaunchBrowser(bool is_machine, BrowserType type, const CString& url) {
  CORE_LOG(L3, (_T("[LaunchBrowser][%d][%u][%s]"), is_machine, type, url));

  if (is_machine && vista_util::IsVistaOrLater() && vista_util::IsUserAdmin()) {
    // Other than having a service launch the browser using CreateProcessAsUser,
    // there is no easy solution if we are unable to launch the browser
    // impersonated.
    return LaunchImpersonatedBrowser(type, url);
  }

  HRESULT hr = RunBrowser(type, url);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[RunBrowser failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

CString BuildGoogleUpdateExeDir(bool is_machine) {
  ConfigManager& cm = *ConfigManager::Instance();
  return is_machine ? cm.GetMachineGoopdateInstallDir() :
                      cm.GetUserGoopdateInstallDir();
}

CString BuildGoogleUpdateExePath(bool is_machine) {
  CORE_LOG(L3, (_T("[BuildGoogleUpdateExePath][%d]"), is_machine));

  CPath full_file_path(BuildGoogleUpdateExeDir(is_machine));
  VERIFY1(full_file_path.Append(kOmahaShellFileName));

  return full_file_path;
}

CString BuildGoogleUpdateServicesPath(bool is_machine) {
  CORE_LOG(L3, (_T("[BuildGoogleUpdateServicesPath][%d]"), is_machine));

  CPath full_file_path(BuildInstallDirectory(is_machine, GetVersionString()));
  VERIFY1(full_file_path.Append(kCrashHandlerFileName));

  return full_file_path;
}

HRESULT StartElevatedSelfWithArgsAndWait(const TCHAR* args, DWORD* exit_code) {
  ASSERT1(args);
  ASSERT1(exit_code);
  CORE_LOG(L3, (_T("[StartElevatedSelfWithArgsAndWait]")));

  // Get the process executable.
  TCHAR filename[MAX_PATH] = {0};
  if (::GetModuleFileName(NULL, filename, MAX_PATH) == 0) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LEVEL_ERROR, (_T("[GetModuleFileName failed][0x%08x]"), hr));
    return hr;
  }

  // Launch self elevated and wait.
  *exit_code = 0;
  CORE_LOG(L1,
      (_T("[RunElevated filename='%s'][arguments='%s']"), filename, args));
  // According to the MSDN documentation for ::ShowWindow: "nCmdShow. This
  // parameter is ignored the first time an application calls ShowWindow, if
  // the program that launched the application provides a STARTUPINFO
  // structure.". We want to force showing the UI window. So we pass in
  // SW_SHOWNORMAL.
  HRESULT hr(vista_util::RunElevated(filename, args, SW_SHOWNORMAL, exit_code));
  CORE_LOG(L2, (_T("[elevated instance exit code][%u]"), *exit_code));
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[RunElevated failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT StartGoogleUpdateWithArgs(bool is_machine,
                                  const TCHAR* args,
                                  HANDLE* process) {
  CORE_LOG(L3, (_T("[StartGoogleUpdateWithArgs][%d][%s]"),
                is_machine, args ? args : _T("")));

  CString exe_path = BuildGoogleUpdateExePath(is_machine);

  CORE_LOG(L3, (_T("[command line][%s][%s]"), exe_path, args ? args : _T("")));

  HRESULT hr = System::ShellExecuteProcess(exe_path, args, NULL, process);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[can't start process][%s][0x%08x]"), exe_path, hr));
    return hr;
  }
  return S_OK;
}

HRESULT StartCrashHandler(bool is_machine) {
  CORE_LOG(L3, (_T("[StartCrashHandler]")));

  ASSERT1(!is_machine || user_info::IsRunningAsSystem());

  CString exe_path = BuildGoogleUpdateServicesPath(is_machine);
  CommandLineBuilder builder(COMMANDLINE_MODE_CRASH_HANDLER);
  CString cmd_line = builder.GetCommandLineArgs();
  return System::StartProcessWithArgs(exe_path, cmd_line);
}

bool IsRunningFromOfficialGoopdateDir(bool is_machine) {
  const ConfigManager& cm = *ConfigManager::Instance();
  bool is_official_dir = is_machine ?
                         cm.IsRunningFromMachineGoopdateInstallDir() :
                         cm.IsRunningFromUserGoopdateInstallDir();
  CORE_LOG(L3, (_T("[running from official dir][%d]"), is_official_dir));
  return is_official_dir;
}

CString GetHKRoot() {
  return IsRunningFromOfficialGoopdateDir(true) ? _T("HKLM") : _T("HKCU");
}

HRESULT InitializeSecurity() {
  // Creates a security descriptor in absolute format and includes the owner
  // and the primary group.  We grant access to admins and system.
  CSecurityDesc security_descriptor;
  if (SystemInfo::IsRunningOnVistaOrLater()) {
    // To allow for low-integrity IE to call into IGoogleUpdate.
    security_descriptor.FromString(LOW_INTEGRITY_SDDL_SACL);
  }
  security_descriptor.SetOwner(Sids::Admins());
  security_descriptor.SetGroup(Sids::Admins());
  CDacl dacl;
  dacl.AddAllowedAce(Sids::System(), COM_RIGHTS_EXECUTE);
  dacl.AddAllowedAce(Sids::Admins(), COM_RIGHTS_EXECUTE);
  dacl.AddAllowedAce(Sids::AuthenticatedUser(), COM_RIGHTS_EXECUTE);

  security_descriptor.SetDacl(dacl);
  security_descriptor.MakeAbsolute();

  SECURITY_DESCRIPTOR* sd = const_cast<SECURITY_DESCRIPTOR*>(
      security_descriptor.GetPSECURITY_DESCRIPTOR());

  return ::CoInitializeSecurity(
      sd,
      -1,
      NULL,   // Let COM choose what authentication services to register.
      NULL,
      RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  // Data integrity and encryption.
      RPC_C_IMP_LEVEL_IDENTIFY,       // Only allow a server to identify.
      NULL,
      EOAC_DYNAMIC_CLOAKING | EOAC_NO_CUSTOM_MARSHAL,
      NULL);
}

// This is only used for legacy handoff support.
CString GetProductName(const CString& app_guid) {
  const TCHAR* product_name = NULL;
  const TCHAR gears_guid[]   = _T("{283EAF47-8817-4c2b-A801-AD1FADFB7BAA}");
  const TCHAR google_talk_plugin[]  =
      _T("{D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}");
  const TCHAR youtube_uploader_guid[] =
      _T("{A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}");

  if (app_guid.CompareNoCase(gears_guid) == 0) {
    product_name = _T("Gears");
  } else if (app_guid.CompareNoCase(google_talk_plugin) == 0) {
      product_name = _T("Google Talk Plugin");
  } else if (app_guid.CompareNoCase(youtube_uploader_guid) == 0) {
      product_name = _T("YouTube Uploader");
  } else {
      product_name = _T("Google App");
  }
  return product_name;
}

HRESULT RedirectHKCR(bool is_machine) {
  RegKey classes_key;
  HRESULT hr = classes_key.Open(is_machine ?
                                HKEY_LOCAL_MACHINE :
                                HKEY_CURRENT_USER,
                                _T("Software\\Classes"),
                                KEY_ALL_ACCESS);
  if (FAILED(hr)) {
    ASSERT(FALSE, (_T("RedirectHKCR - key.Open(%d) fail %d"), is_machine, hr));
    return hr;
  }

  LONG result = ::RegOverridePredefKey(HKEY_CLASSES_ROOT, classes_key.Key());
  if (result != ERROR_SUCCESS) {
    ASSERT(false, (_T("RedirectHKCR - RegOverridePredefKey fail %d"), result));
    return HRESULT_FROM_WIN32(result);
  }

  return S_OK;
}

HRESULT RemoveRedirectHKCR() {
  LONG result = ::RegOverridePredefKey(HKEY_CLASSES_ROOT, NULL);
  if (result != ERROR_SUCCESS) {
    ASSERT(FALSE, (_T("RemoveRedirectHKCR - RegOverridePredefKey %d"), result));
    return HRESULT_FROM_WIN32(result);
  }

  return S_OK;
}

HRESULT RegisterTypeLib(bool is_admin,
                        const CComBSTR& path,
                        ITypeLib* type_lib) {
  // Typelib registration.
  CORE_LOG(L3, (_T("[Registering TypeLib]")));
  HRESULT hr = S_OK;
  if (!is_admin &&
      SUCCEEDED(goopdate_utils::RegisterTypeLibForUser(type_lib, path, NULL))) {
    return S_OK;
  }

  // For Admin cases, we use ::RegisterTypeLib().
  // For platforms where ::RegisterTypeLibForUser is not available, we register
  // with ::RegisterTypeLib, and rely on HKCR=>HKCU redirection.
  hr = ::RegisterTypeLib(type_lib, path, NULL);
  ASSERT(SUCCEEDED(hr), (_T("[TypeLib registration failed][0x%08x]"), hr));
  return hr;
}

HRESULT UnRegisterTypeLib(bool is_admin, const CComBSTR&, ITypeLib* type_lib) {
  // Typelib unregistration.
  CORE_LOG(L3, (_T("[Unregistering Typelib]")));
  TLIBATTR* tlib_attr = NULL;
  HRESULT hr = type_lib->GetLibAttr(&tlib_attr);
  ASSERT(SUCCEEDED(hr), (_T("[GetLibAttr failed][0x%08x]"), hr));
  if (FAILED(hr)) {
    return hr;
  }
  ON_SCOPE_EXIT_OBJ(*type_lib, &ITypeLib::ReleaseTLibAttr, tlib_attr);

  if (!is_admin &&
      SUCCEEDED(goopdate_utils::UnRegisterTypeLibForUser(
          tlib_attr->guid,
          tlib_attr->wMajorVerNum,
          tlib_attr->wMinorVerNum,
          tlib_attr->lcid,
          tlib_attr->syskind))) {
    return S_OK;
  }

  // For Admin cases, we use ::UnRegisterTypeLib().
  // For platforms where ::UnRegisterTypeLibForUser is not available, we
  // unregister with ::UnRegisterTypeLib, and rely on HKCR=>HKCU redirection.
  hr = ::UnRegisterTypeLib(tlib_attr->guid,
                           tlib_attr->wMajorVerNum,
                           tlib_attr->wMinorVerNum,
                           tlib_attr->lcid,
                           tlib_attr->syskind);

  // We assert before the check for TYPE_E_REGISTRYACCESS below because we want
  // to catch the case where we're trying to unregister more than once because
  // that would be a bug.
  ASSERT(SUCCEEDED(hr),
         (_T("[UnRegisterTypeLib failed.  ")
          _T("This is likely a multiple unregister bug.][0x%08x]"), hr));

  // If you try to unregister a type library that's already unregistered,
  // it will return with this failure, which is OK.
  if (hr == TYPE_E_REGISTRYACCESS) {
    hr = S_OK;
  }

  return hr;
}

HRESULT RegisterOrUnregisterModule(bool is_machine,
                                   bool register_server,
                                   RegisterOrUnregisterFunction registrar,
                                   void* data) {
  ASSERT1(registrar);

  // ATL by default registers the control to HKCR and we want to register
  // either in HKLM, or in HKCU, depending on whether we are laying down
  // the system googleupdate, or the user googleupdate.
  // We solve this for the user goopdate case by:
  // * Having the RGS file take a HKROOT parameter that translates to either
  //   HKLM or HKCU.
  // * Redirecting HKCR to HKCU\software\classes, for a user installation, to
  //   cover Proxy registration.
  // For the machine case, we still redirect HKCR to HKLM\\Software\\Classes,
  // to ensure that Proxy registration happens in HKLM.
  HRESULT hr = RedirectHKCR(is_machine);
  ASSERT1(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }
  // We need to stop redirecting at the end of this function.
  ON_SCOPE_EXIT(RemoveRedirectHKCR);

  hr = (*registrar)(data, register_server);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[RegisterOrUnregisterModule failed][%d][0x%08x]"),
                  register_server, hr));
    ASSERT1(!register_server);
  }

  return hr;
}

HRESULT RegisterOrUnregisterModuleWithTypelib(
    bool is_machine,
    bool register_server,
    RegisterOrUnregisterFunction registrar,
    void* data) {
  ASSERT1(registrar);

  // By default, ATL registers the control to HKCR and we want to register
  // either in HKLM, or in HKCU, depending on whether we are laying down
  // the machine googleupdate, or the user googleupdate.
  // We solve this for the user goopdate case by:
  // * Having the RGS file take a HKROOT parameter that translates to either
  //   HKLM or HKCU.
  // * Redirecting HKCR to HKCU\software\classes, for a user installation, to
  //   cover AppId and TypeLib registration
  // * All the above makes ATL work correctly for 2K/XP. However on Win2K3
  //   and Vista, redirection does not work by itself, because in these
  //   platforms, RegisterTypeLib writes explicitly to HKLM\Software\Classes.
  //   We need to specifically call the new RegisterTypeLibForUser() API.
  //   So, we do that as well.
  // For the machine case, we still redirect HKCR to HKLM\\Software\\Classes,
  // because otherwise RegisterTypeLib ends up overwriting HKCU if the key
  // already exists in HKCU.
  HRESULT hr = RedirectHKCR(is_machine);
  ASSERT1(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }
  // We need to stop redirecting at the end of this function.
  ON_SCOPE_EXIT(RemoveRedirectHKCR);

  // load the type library.
  CComPtr<ITypeLib> type_lib;
  CComBSTR path;
  hr = ::AtlLoadTypeLib(_AtlBaseModule.GetModuleInstance(), NULL, &path,
                        &type_lib);
  if (FAILED(hr)) {
    ASSERT(false, (_T("[AtlLoadTypeLib failed][0x%08x]"), hr));
    return hr;
  }

  if (register_server) {
    hr = (*registrar)(data, register_server);
    if (FAILED(hr)) {
      ASSERT(false, (_T("[Module registration failed][0x%08x]"), hr));
      return hr;
    }

    return RegisterTypeLib(is_machine, path, type_lib);
  } else {
    hr = UnRegisterTypeLib(is_machine, path, type_lib);
    if (FAILED(hr)) {
      ASSERT(false, (_T("[UnRegisterTypeLib failed][0x%08x]"), hr));
      return hr;
    }

    return (*registrar)(data, register_server);
  }
}

HRESULT RegisterTypeLibForUser(ITypeLib* lib,
                               OLECHAR* path,
                               OLECHAR* help_dir) {
  CORE_LOG(L3, (_T("[RegisterTypeLibForUser]")));
  ASSERT1(lib);
  ASSERT1(path);
  // help_dir can be NULL.

  const TCHAR* library_name = _T("oleaut32.dll");
  scoped_library module(static_cast<HINSTANCE>(::LoadLibrary(library_name)));
  if (!module) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LEVEL_ERROR,
        (_T("[LoadLibrary failed][%s][0x%08x]"), library_name, hr));
    return hr;
  }

  // RegisterTypeLibForUser function from oleaut32.dll.
  typedef HRESULT(__stdcall *PF)(ITypeLib*, OLECHAR*, OLECHAR*);

  const char* function_name = "RegisterTypeLibForUser";
  PF fp = reinterpret_cast<PF>(::GetProcAddress(get(module), function_name));
  if (!fp) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LEVEL_ERROR,
             (_T("[GetProcAddress failed][%s][0x%08x]"),
              function_name, library_name, hr));
    return hr;
  }

  CORE_LOG(L3, (_T("[Calling RegisterTypelibForUser in oleaut]")));
  HRESULT hr = fp(lib, path, help_dir);
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[regtypelib_for_user failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT UnRegisterTypeLibForUser(REFGUID lib_id,
                                 WORD major_ver_num,
                                 WORD minor_ver_num,
                                 LCID lcid,
                                 SYSKIND syskind) {
  CORE_LOG(L3, (_T("[UnRegisterTypeLibForUser]")));

  const TCHAR* library_name = _T("oleaut32.dll");
  scoped_library module(static_cast<HINSTANCE>(::LoadLibrary(library_name)));
  if (!module) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LEVEL_ERROR,
        (_T("[LoadLibrary failed][%s][0x%08x]"), library_name, hr));
    return hr;
  }

  // UnRegisterTypeLibForUser function from oleaut32.dll.
  typedef HRESULT (__stdcall *PF)(REFGUID, WORD, WORD, LCID, SYSKIND);

  const char* function_name = "UnRegisterTypeLibForUser";
  PF fp = reinterpret_cast<PF>(::GetProcAddress(get(module), function_name));
  if (!fp) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LEVEL_ERROR,
             (_T("[GetProcAddress failed][%s][0x%08x]"),
              function_name, library_name, hr));
    return hr;
  }

  CORE_LOG(L3, (_T("[Calling UnRegisterTypeLibForUser in oleaut]")));
  HRESULT hr = fp(lib_id, major_ver_num, minor_ver_num, lcid, syskind);
  if (FAILED(hr)) {
    CORE_LOG(LEVEL_ERROR, (_T("[unregtypelib_for_user failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

// TODO(omaha): This method's name is much more specific than what it does. Can
// we just copy the code to scheduled task and service code and eliminate it?
// Reads the current value under {HKLM|HKCU}\Google\Update\value_name. Returns
// default_val if value_name does not exist.
CString GetCurrentVersionedName(bool is_machine,
                                const TCHAR* value_name,
                                const TCHAR* default_val) {
  CORE_LOG(L3, (_T("[ConfigManager::GetCurrentVersionedName]")));
  ASSERT1(value_name && *value_name);
  ASSERT1(default_val && *default_val);

  const TCHAR* key_name = is_machine ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  CString name;
  HRESULT hr(RegKey::GetValue(key_name, value_name, &name));
  if (FAILED(hr)) {
    CORE_LOG(L4, (_T("[GetValue failed][%s][0x%x][Using default name][%s]"),
                  value_name, hr, default_val));
    name = default_val;
  }

  CORE_LOG(L3, (_T("[Versioned Name][%s]"), name));
  return name;
}

// Creates a unique name of the form "{prefix}1c9b3d6baf90df3" and stores it in
// the registry under HKLM/HKCU\Google\Update\value_name. Subsequent
// invocations of GetCurrentTaskName() will return this new value.
HRESULT CreateAndSetVersionedNameInRegistry(bool is_machine,
                                            const TCHAR* prefix,
                                            const TCHAR* value_name) {
  ASSERT1(prefix && *prefix);
  ASSERT1(value_name && *value_name);

  // TODO(omaha): Move from service_utils.h since it is used for other purposes.
  CString name(ServiceInstall::GenerateServiceName(prefix));
  CORE_LOG(L3, (_T("Versioned name[%s][%s][%s]"), prefix, value_name, name));

  const TCHAR* key_name = is_machine ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  return RegKey::SetValue(key_name, value_name, name);
}

HRESULT TerminateAllBrowsers(
    BrowserType type,
    TerminateBrowserResult* browser_res,
    TerminateBrowserResult* default_res) {
  UTIL_LOG(L3, (_T("[TerminateAllBrowsers][%d]"), type));
  ASSERT1(default_res);
  ASSERT1(browser_res);

  if (type == BROWSER_UNKNOWN ||
      type == BROWSER_DEFAULT ||
      type >= BROWSER_MAX) {
    ASSERT1(false);
    return E_INVALIDARG;
  }

  const BrowserType kFirstBrowser = BROWSER_IE;
  const int kNumSupportedBrowsers = BROWSER_MAX - kFirstBrowser;

  BrowserType default_type = BROWSER_UNKNOWN;
  HRESULT hr = GetDefaultBrowserType(&default_type);
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[GetDefaultBrowserType failed][0x%08x]"), hr));
    return hr;
  }

  TerminateBrowserResult terminate_results[kNumSupportedBrowsers];

  for (int browser = 0; browser < kNumSupportedBrowsers; ++browser) {
    const BrowserType browser_type =
        static_cast<BrowserType>(kFirstBrowser + browser);
    hr = TerminateBrowserProcess(browser_type,
                                 CString(),
                                 0,
                                 &terminate_results[browser].found);
    if (FAILED(hr)) {
      UTIL_LOG(LW, (_T("[TerminateBrowserProcess failed][%u][0x%08x]"),
                    browser_type, hr));
    }
  }

  // Now wait for the all browser instances to die.
  // TODO(omaha): Wait for all processes at once rather than waiting for
  // (kTerminateBrowserTimeoutMs * # supported browsers) ms.
  for (int browser = 0; browser < kNumSupportedBrowsers; ++browser) {
    const BrowserType browser_type =
        static_cast<BrowserType>(kFirstBrowser + browser);
    hr = WaitForBrowserToDie(browser_type,
                             CString(),
                             kTerminateBrowserTimeoutMs);
    if (FAILED(hr)) {
      UTIL_LOG(LW, (_T("[WaitForBrowserToDie failed][%u][0x%08x]"),
                    browser_type, hr));
    } else {
      terminate_results[browser].could_terminate = true;
    }
  }

  *browser_res = terminate_results[type - kFirstBrowser];
  *default_res = terminate_results[default_type - kFirstBrowser];

  return S_OK;
}

// default_type can be BROWSER_UNKNOWN.
// If browsers that must be closed could not be terminated, false is returned.
// This method and TerminateBrowserProcesses assume the user did not shutdown
// the specified browser. They restart and shutdown, respectively, the default
// browser when the specified browser is not found. The reason for this may have
// been that the the specified (stamped) browser could be in a bad state on the
// machine and trying to start it would fail.
// This may also be required to support hosted cases (i.e. AOL and Maxthon).
// TODO(omaha): If we assume the stamped browser is okay, check whether the
// specified browser is installed rather than relying on whether the browser was
// running. It is perfectly valid for the browser to not be running.
// TODO(omaha): Why not try the default browser if browsers that require
// shutdown failed to terminate.
bool GetBrowserToRestart(BrowserType type,
                         BrowserType default_type,
                         const TerminateBrowserResult& res,
                         const TerminateBrowserResult& def_res,
                         BrowserType* browser_type) {
  ASSERT1(browser_type);
  ASSERT1(type != BROWSER_UNKNOWN &&
          type != BROWSER_DEFAULT &&
          type < BROWSER_MAX);
  ASSERT1(default_type != BROWSER_DEFAULT && default_type < BROWSER_MAX);
  UTIL_LOG(L3, (_T("[GetBrowserToRestart][%d]"), type));

  *browser_type = BROWSER_UNKNOWN;

  if (res.found) {
    switch (type) {
      case BROWSER_IE:
        *browser_type = BROWSER_IE;
        return true;
      case BROWSER_FIREFOX:   // Only one process.
      case BROWSER_CHROME:    // One process per plug-in, even for upgrades.
        if (res.could_terminate) {
          *browser_type = type;
          return true;
        }
        return false;
      case BROWSER_UNKNOWN:
      case BROWSER_DEFAULT:
      case BROWSER_MAX:
      default:
        break;
    }
  }

  // We did not find the browser that we wanted to restart. Hence we need to
  // determine if we could shutdown the default browser.
  switch (default_type) {
    case BROWSER_IE:
      *browser_type = BROWSER_IE;
      return true;
    case BROWSER_FIREFOX:
    case BROWSER_CHROME:
      if (!def_res.found || def_res.found && def_res.could_terminate) {
        *browser_type = default_type;
        return true;
      }
      break;
    case BROWSER_UNKNOWN:
    case BROWSER_DEFAULT:
    case BROWSER_MAX:
    default:
      break;
  }

  return false;
}

// See the comments about the default browser above GetBrowserToRestart.
HRESULT TerminateBrowserProcesses(BrowserType type,
                                  TerminateBrowserResult* browser_res,
                                  TerminateBrowserResult* default_res) {
  UTIL_LOG(L3, (_T("[TerminateBrowserProcesses][%d]"), type));
  ASSERT1(browser_res);
  ASSERT1(default_res);

  browser_res->could_terminate = false;
  default_res->could_terminate = false;

  if (type == BROWSER_UNKNOWN ||
      type == BROWSER_DEFAULT ||
      type >= BROWSER_MAX) {
    ASSERT1(false);
    return E_UNEXPECTED;
  }

  CString sid;
  HRESULT hr = user_info::GetProcessUser(NULL, NULL, &sid);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetProcessUser failed][0x%08x]"), hr));
    return hr;
  }

  hr = TerminateBrowserProcess(type,
                               sid,
                               kTerminateBrowserTimeoutMs,
                               &browser_res->found);
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[TerminateBrowserProcess failed][0x%08x]"), hr));
  } else {
    browser_res->could_terminate = true;
  }

  // Since no instances of the browser type exist, we try to find and kill
  // all instances of the default browser.
  if (!browser_res->found) {
    // We dont want to try and terminate the default browser, if it is the
    // same as the browser that we tried above.

    BrowserType default_type = BROWSER_UNKNOWN;
    hr = GetDefaultBrowserType(&default_type);
    if (FAILED(hr)) {
      UTIL_LOG(LW, (_T("[GetDefaultBrowserType failed][0x%08x]"), hr));
    }

    UTIL_LOG(L3, (_T("[Trying to kill the default browser %d]"), default_type));
    if (default_type != type) {
      hr = TerminateBrowserProcess(BROWSER_DEFAULT,
                                   sid,
                                   kTerminateBrowserTimeoutMs,
                                   &default_res->found);
      if (FAILED(hr)) {
        UTIL_LOG(LW, (_T("[TerminateBrowserProcess failed][0x%08x]"), hr));
      } else {
        default_res->could_terminate = true;
      }
    }
  }

  return hr;
}

HRESULT GetBrowserImagePathFromProcess(BrowserType type,
                                       uint32 explorer_pid,
                                       CString* path) {
  ASSERT1(path);

  if (type == BROWSER_UNKNOWN || type >= BROWSER_MAX) {
    ASSERT1(false);
    return E_UNEXPECTED;
  }

  if (type == BROWSER_DEFAULT) {
    return GetDefaultBrowserPath(path);
  }

  CString user_sid;
  HRESULT hr = Process::GetProcessOwner(explorer_pid, &user_sid);
  if (FAILED(hr)) {
    UTIL_LOG(LEVEL_WARNING, (_T("[GetProcessOwner failed.][0x%08x]"), hr));
    return hr;
  }

  CString browser_name;
  hr = BrowserTypeToProcessName(type, &browser_name);
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[BrowserTypeToProcessName failed.][0x%08x]"), hr));
    return hr;
  }

  hr = Process::GetImagePath(browser_name, user_sid, path);
  if (FAILED(hr)) {
    UTIL_LOG(LW, (_T("[GetImagePath failed.][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT ConvertStringToBrowserType(const CString& text, BrowserType* type) {
  ASSERT1(type != NULL);

  if (text.GetLength() != 1) {
    return GOOPDATEUTILS_E_BROWSERTYPE;
  }

  int browser_type = 0;
  if (!String_StringToDecimalIntChecked(text, &browser_type)) {
    return GOOPDATEUTILS_E_BROWSERTYPE;
  }

  if (browser_type >= BROWSER_MAX) {
    return GOOPDATEUTILS_E_BROWSERTYPE;
  }

  *type = static_cast<BrowserType>(browser_type);
  return S_OK;
}

CString ConvertBrowserTypeToString(BrowserType type) {
  CString text = itostr(static_cast<int>(type));
  ASSERT1(!text.IsEmpty());
  return text;
}

HRESULT GetOSInfo(CString* os_version, CString* service_pack) {
  ASSERT1(os_version);
  ASSERT1(service_pack);

  OSVERSIONINFO os_version_info = { 0 };
  os_version_info.dwOSVersionInfoSize = sizeof(os_version_info);
  if (!::GetVersionEx(&os_version_info)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LW, (_T("[GetVersionEx failed][0x%08x]"), hr));
    return hr;
  }

  os_version->Format(_T("%d.%d"),
                     os_version_info.dwMajorVersion,
                     os_version_info.dwMinorVersion);
  *service_pack = os_version_info.szCSDVersion;
  return S_OK;
}

CPath BuildInstallDirectory(bool is_machine, const CString& version) {
  ConfigManager& cm = *ConfigManager::Instance();
  CPath install_dir(is_machine ? cm.GetMachineGoopdateInstallDir() :
                                 cm.GetUserGoopdateInstallDir());
  VERIFY1(install_dir.Append(version));

  return install_dir;
}

// This method does a very specific job of searching for install workers,
// for the user and machine omaha. It also includes the on-demand updates COM
// server, because we treat it similar to interactive installs, and selfupdate.
//
// In machine case we search in all the accounts since the install worker can be
// running in any admin account and the machine update worker runs as SYSTEM.
// In the user case, we only search the user's account.
// In both cases, the Needsadmin command line parameter is checked for
// true/false in the machine/user case, respectively.
//
// Only adds processes to the input vector; does not clear it.
//
// TODO(omaha): For now we search for the needs_admin=true in the command
// line to determine a machine install. Another option of identifying omaha's
// is to use the presence of a named mutex. So the user omaha will create
// Global\<sid>\Mutex and the machine will create Global\Mutex, in here then
// we can test for the presence of the name to decide if an interactive
// omaha is running.
// TODO(omaha): Consider further filtering the processes based on whether
// the owner is elevated in case of machine omaha.
// Looks for the /ig command line used in Omaha 2.
HRESULT GetInstallWorkerProcesses(bool is_machine,
                                  std::vector<uint32>* processes) {
  ASSERT1(processes);

  CString user_sid;
  DWORD flags = EXCLUDE_CURRENT_PROCESS |
                EXCLUDE_PARENT_PROCESS  |
                INCLUDE_PROCESS_COMMAND_LINE_CONTAINING_STRING;

  std::vector<CString> command_lines;
  CString command_line_to_include;
  command_line_to_include.Format(_T("/%s"), kCmdLineInstall);
  command_lines.push_back(command_line_to_include);
  command_line_to_include.Format(_T("/%s"), kCmdLineInstallElevated);
  command_lines.push_back(command_line_to_include);
  command_line_to_include.Format(_T("/%s"), kCmdLineAppHandoffInstall);
  command_lines.push_back(command_line_to_include);
  command_line_to_include.Format(_T("/%s"), kCmdLineUpdate);
  command_lines.push_back(command_line_to_include);
  command_line_to_include.Format(_T("/%s"),
                                 kCmdLineLegacyFinishGoogleUpdateInstall);
  command_lines.push_back(command_line_to_include);
  command_lines.push_back(kCmdLineComServerDash);

  if (!is_machine) {
    // Search only the same sid as the current user.
    flags |= INCLUDE_ONLY_PROCESS_OWNED_BY_USER;

    HRESULT hr = user_info::GetProcessUser(NULL, NULL, &user_sid);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetProcessUser failed][0x%08x]"), hr));
      return hr;
    }
  }

  std::vector<uint32> all_install_worker_processes;
  HRESULT hr = Process::FindProcesses(flags,
                                      kOmahaShellFileName,
                                      true,
                                      user_sid,
                                      command_lines,
                                      &all_install_worker_processes);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[FindProcesses failed][0x%08x]"), hr));
    return hr;
  }

  CString official_path;
  hr = GetFolderPath(is_machine ? CSIDL_PROGRAM_FILES : CSIDL_LOCAL_APPDATA,
                     &official_path);
  ASSERT1(SUCCEEDED(hr));
  ASSERT1(!official_path.IsEmpty());

  for (size_t i = 0; i < all_install_worker_processes.size(); ++i) {
    CString cmd_line;
    const uint32 process = all_install_worker_processes[i];
    if (SUCCEEDED(Process::GetCommandLine(process, &cmd_line))) {
      cmd_line.MakeLower();
      // TODO(omaha): FindProcess method does not allow regex's to be specified
      // along with the include command line. Change Process to allow this.
      if (cmd_line.Find(is_machine ? kNeedsAdminYes : kNeedsAdminNo) != -1) {
        CORE_LOG(L4, (_T("[Including process][%s]"), cmd_line));
        processes->push_back(process);
      }

      // A needsadmin=prefers instance could be installing either for machine or
      // for user.
      if (cmd_line.Find(kNeedsAdminPrefers) != -1) {
        CORE_LOG(L4, (_T("[Including process][%s]"), cmd_line));
        processes->push_back(process);
      }

      // The -Embedding does not have a needsAdmin. Decide whether to include it
      // if it matches the official path for the requested instance type.
      CString exe_path;
      if (cmd_line.Find(kCmdLineComServerDash) != -1 &&
          SUCCEEDED(GetExePathFromCommandLine(cmd_line, &exe_path)) &&
          String_StrNCmp(official_path, exe_path, official_path.GetLength(),
                         true) == 0) {
        CORE_LOG(L4, (_T("[Including process][%s]"), cmd_line));
        processes->push_back(process);
      }
    }
  }

  return S_OK;
}

// The event name saved to the environment variable does not contain the
// decoration added by GetNamedObjectAttributes.
HRESULT CreateUniqueEventInEnvironment(const CString& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event) {
  ASSERT1(unique_event);

  GUID event_guid = GUID_NULL;
  HRESULT hr = ::CoCreateGuid(&event_guid);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[::CoCreateGuid failed][0x%08x]"), hr));
    return hr;
  }

  CString event_name(GuidToString(event_guid));
  NamedObjectAttributes attr;
  GetNamedObjectAttributes(event_name, is_machine, &attr);

  hr = CreateEvent(&attr, unique_event);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[CreateEvent failed in CreateUniqueEventInEnvironment]"),
                  _T("[%s][0x%08x]"), var_name, hr));
    return hr;
  }

  CORE_LOG(L3, (_T("[created unique event][%s][%s]"), var_name, event_name));

  if (!::SetEnvironmentVariable(var_name, event_name)) {
    DWORD error = ::GetLastError();
    CORE_LOG(LE, (_T("[::SetEnvironmentVariable failed][%d]"), error));
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

HRESULT OpenUniqueEventFromEnvironment(const CString& var_name,
                                       bool is_machine,
                                       HANDLE* unique_event) {
  ASSERT1(unique_event);

  TCHAR event_name[MAX_PATH] = {0};
  if (!::GetEnvironmentVariable(var_name, event_name, arraysize(event_name))) {
    DWORD error = ::GetLastError();
    CORE_LOG(LW, (_T("[Failed to read environment variable][%s][%d]"),
                  var_name, error));
    return HRESULT_FROM_WIN32(error);
  }

  CORE_LOG(L3, (_T("[read unique event][%s][%s]"), var_name, event_name));

  NamedObjectAttributes attr;
  GetNamedObjectAttributes(event_name, is_machine, &attr);
  *unique_event = ::OpenEvent(EVENT_ALL_ACCESS, false, attr.name);

  if (!*unique_event) {
    DWORD error = ::GetLastError();
    CORE_LOG(LW, (_T("[::OpenEvent failed][%s][%d]"), attr.name, error));
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

// The caller is responsible for reseting the event and closing the handle.
HRESULT CreateEvent(NamedObjectAttributes* event_attr, HANDLE* event_handle) {
  ASSERT1(event_handle);
  ASSERT1(event_attr);
  ASSERT1(!event_attr->name.IsEmpty());
  *event_handle = ::CreateEvent(&event_attr->sa,
                                true,   // manual reset
                                false,  // not signaled
                                event_attr->name);

  if (!*event_handle) {
    DWORD error = ::GetLastError();
    CORE_LOG(LEVEL_ERROR, (_T("[::CreateEvent failed][%d]"), error));
    return HRESULT_FROM_WIN32(error);
  }

  return S_OK;
}

bool IsTestSource() {
  return !ConfigManager::Instance()->GetTestSource().IsEmpty();
}

HRESULT ReadNameValuePairsFromFile(const CString& file_path,
                                   const CString& group_name,
                                   std::map<CString, CString>* pairs) {
  ASSERT1(pairs);

  if (!File::Exists(file_path)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  pairs->clear();

  TCHAR str_buf[32768] = {0};

  // Retrieve all key names in the section requested.
  DWORD buf_count = ::GetPrivateProfileString(group_name,
                                              NULL,
                                              NULL,
                                              str_buf,
                                              arraysize(str_buf),
                                              file_path);

  DWORD offset = 0;
  while (offset < buf_count) {
    TCHAR val_buf[1024] = {0};
    CString current_key = &(str_buf[offset]);
    DWORD val_count = ::GetPrivateProfileString(group_name,
                                                current_key,
                                                NULL,
                                                val_buf,
                                                arraysize(val_buf),
                                                file_path);
    (*pairs)[current_key] = val_buf;
    offset += current_key.GetLength() + 1;
  }

  return S_OK;
}

HRESULT WriteNameValuePairsToFile(const CString& file_path,
                                  const CString& group_name,
                                  const std::map<CString, CString>& pairs) {
  std::map<CString, CString>::const_iterator it = pairs.begin();
  for (; it != pairs.end(); ++it) {
    if (!::WritePrivateProfileString(group_name,
                                     it->first,
                                     it->second,
                                     file_path)) {
      return HRESULTFromLastError();
    }
  }

  return S_OK;
}

bool IsAppInstallWorkerRunning(bool is_machine) {
  CORE_LOG(L3, (_T("[IsAppInstallWorkerRunning][%d]"), is_machine));
  std::vector<uint32> processes;
  VERIFY1(SUCCEEDED(GetInstallWorkerProcesses(is_machine, &processes)));
  return !processes.empty();
}

// Returns true if the version does not begin with "1.0." or "1.1.".
bool IsGoogleUpdate2OrLater(const CString& version) {
  const ULONGLONG kFirstOmaha2Version = MAKEDLLVERULL(1, 2, 0, 0);
  ULONGLONG version_number = VersionFromString(version);
  ASSERT1(0 != version_number);

  if (kFirstOmaha2Version <= version_number) {
    return true;
  }

  return false;
}

HRESULT WriteInstallerDataToTempFile(const CString& installer_data,
                                     CString* installer_data_file_path) {
  ASSERT1(installer_data_file_path);

  // TODO(omaha): consider eliminating the special case and simply create an
  // empty file.
  CORE_LOG(L2, (_T("[WriteInstallerDataToTempFile][data=%s]"), installer_data));
  if (installer_data.IsEmpty()) {
    return S_FALSE;
  }

  CString temp_file;
  if (!::GetTempFileName(app_util::GetTempDir(),
                         _T("gui"),
                         0,
                         CStrBuf(temp_file, MAX_PATH))) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[::GetTempFileName failed][0x08%x]"), hr));
    return hr;
  }

  scoped_hfile file_handle(::CreateFile(temp_file,
                                        GENERIC_WRITE,
                                        FILE_SHARE_READ,
                                        NULL,
                                        CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL));
  if (!file_handle) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[::CreateFile failed][0x08%x]"), hr));
    return hr;
  }

  CStringA installer_data_utf8_bom;
  SafeCStringAFormat(&installer_data_utf8_bom, "%c%c%c%s",
                     0xEF, 0xBB, 0xBF, WideToUtf8(installer_data));

  DWORD bytes_written = 0;
  if (!::WriteFile(get(file_handle),
                   installer_data_utf8_bom,
                   installer_data_utf8_bom.GetLength(),
                   &bytes_written,
                   NULL)) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[::WriteFile failed][0x08%x]"), hr));
    return hr;
  }

  *installer_data_file_path = temp_file;
  return S_OK;
}

// Returns true if the absolute difference between time moments is greater than
// the interval between update checks.
// Deals with clocks rolling backwards, in scenarios where the clock indicates
// some time in the future, for example next year, last_checked_ is updated to
// reflect that time, and then the clock is adjusted back to present.
bool ShouldCheckForUpdates(bool is_machine) {
  ConfigManager* cm = ConfigManager::Instance();
  bool is_period_overridden = false;
  const int update_interval = cm->GetLastCheckPeriodSec(&is_period_overridden);
  if (0 == update_interval) {
    ASSERT1(is_period_overridden);
    OPT_LOG(L1, (_T("[ShouldCheckForUpdates returned 0][checks disabled]")));
    return false;
  }

  const int time_difference = cm->GetTimeSinceLastCheckedSec(is_machine);

  const bool result = time_difference >= update_interval ? true : false;
  CORE_LOG(L3, (_T("[ShouldCheckForUpdates returned %d][%u]"),
                result, is_period_overridden));
  return result;
}

HRESULT UpdateLastChecked(bool is_machine) {
  // Set the last check value to the current value.
  DWORD now = Time64ToInt32(GetCurrent100NSTime());
  CORE_LOG(L3, (_T("[UpdateLastChecked][now %d]"), now));
  HRESULT hr = ConfigManager::Instance()->SetLastCheckedTime(is_machine, now);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[SetLastCheckedTime failed][0x%08x]"), hr));
    return hr;
  }
  return S_OK;
}

HRESULT LaunchUninstallProcess(bool is_machine) {
  CORE_LOG(L2, (_T("[LaunchUninstallProcess]")));
  CString exe_path = BuildGoogleUpdateExePath(is_machine);
  CommandLineBuilder builder(COMMANDLINE_MODE_UNINSTALL);
  CString cmd_line = builder.GetCommandLineArgs();
  return System::StartProcessWithArgs(exe_path, cmd_line);
}

HANDLE GetImpersonationTokenForMachineProcess(bool is_machine) {
  if (!is_machine) {
    return NULL;
  }

  CAccessToken access_token;
  if (access_token.GetThreadToken(TOKEN_READ)) {
    return NULL;
  }

  bool is_local_system(false);
  VERIFY1(SUCCEEDED(IsSystemProcess(&is_local_system)));
  if (!is_local_system) {
    return NULL;
  }

  HANDLE handle = NULL;
  HRESULT hr = vista::GetLoggedOnUserToken(&handle);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetLoggedOnUserToken failed][0x%x]"), hr));
    return NULL;
  }

  return handle;
}

HRESULT EnableSEHOP(bool enable) {
  CORE_LOG(L3, (_T("[EnableSEHOP][%d]"), enable));
  CString omaha_ifeo_key_path;
  omaha_ifeo_key_path.Format(_T("%s\\%s"),
                             kRegKeyImageFileExecutionOptions,
                             kOmahaShellFileName);
  return enable ?
      RegKey::SetValue(omaha_ifeo_key_path, kRegKeyDisableSEHOPValue,
                       static_cast<DWORD>(0)) :
      RegKey::DeleteValue(omaha_ifeo_key_path, kRegKeyDisableSEHOPValue);
}

DEFINE_METRIC_count(opt_in_uid_generated);
HRESULT CreateUserId(bool is_machine) {
  // Do not create user ID when doing OEM installation - to avoid a large
  // number of machines have the same ID.
  if (oem_install_utils::IsOemInstalling(is_machine)) {
    return E_FAIL;
  }

  GLock user_id_lock;
  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kOptUserIdLock, is_machine, &lock_attr);
  if (!user_id_lock.InitializeWithSecAttr(lock_attr.name, &lock_attr.sa)) {
    return E_FAIL;
  }

  __mutexScope(user_id_lock);
  RegKey update_key;
  const ConfigManager& config_manager = *ConfigManager::Instance();
  HRESULT hr = update_key.Create(config_manager.registry_update(is_machine));
  if (FAILED(hr)) {
    return hr;
  }

  if (update_key.HasValue(kRegValueUserId)) {
    return S_OK;
  }

  CString user_id;
  hr = GetGuid(&user_id);
  if (FAILED(hr)) {
    return hr;
  }

  hr = update_key.SetValue(kRegValueUserId, user_id);
  if (FAILED(hr)) {
    return hr;
  }

  ++metric_opt_in_uid_generated;
  CORE_LOG(L3, (_T("[Create unique user ID: %s]"), user_id));
  return S_OK;
}

void DeleteUserId(bool is_machine) {
  RegKey::DeleteValue(ConfigManager::Instance()->registry_update(is_machine),
                      kRegValueUserId);
}

CString GetUserIdLazyInit(bool is_machine) {
  const ConfigManager& config_manager = *ConfigManager::Instance();
  if (oem_install_utils::IsOemInstalling(is_machine) ||
      !config_manager.CanCollectStats(is_machine)) {
    DeleteUserId(is_machine);
    return CString();
  }

  if (!RegKey::HasValue(config_manager.registry_update(is_machine),
                        kRegValueUserId)) {
    VERIFY1(SUCCEEDED(CreateUserId(is_machine)));
  }

  CString user_id;
  RegKey::GetValue(config_manager.registry_update(is_machine),
                   kRegValueUserId,
                   &user_id);
  return user_id;
}

}  // namespace goopdate_utils

}  // namespace omaha
