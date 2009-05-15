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
// For interactive instances, do not access the network before displaying the
// UI. This provides a better user experience - quick UI - when the network is
// slow. It is also required to ensure that UI displayed event is signaled
// before potentially waiting on a firewall prompt.
//
// Debugging notes:
//  * Omaha (and app) initial install:
//   * File install can be debugged with the following command arguments:
//      /install "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%20Chrome&needsadmin=False&lang=en"  // NOLINT
//      /install "appguid={283EAF47-8817-4c2b-A801-AD1FADFB7BAA}&appname=Gears&needsadmin=True&lang=en"  // NOLINT
//   * If elevation is required, another instance is launched. Run elevated to
//     continue debugging.
//   * Once the initial files have been copied, another instance is launched
//     from the installed location. To continue debugging, use the following
//     command arguments:
//      /ig "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%20Chrome&needsadmin=False&lang=en"  // NOLINT
//      /ig "appguid={283EAF47-8817-4c2b-A801-AD1FADFB7BAA}&appname=Gears&needsadmin=True&lang=en"  // NOLINT
//  * App hand-off initial install or over-install:
//      /handoff "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%20Chrome&needsadmin=False&lang=en"  // NOLINT
//      /handoff "appguid={283EAF47-8817-4c2b-A801-AD1FADFB7BAA}&appname=Gears&needsadmin=True&lang=en"  // NOLINT
//  * Silent install:
//   * Add "/silent" to any of the above command lines (not to the tag).
//  * Google Update self-update:
//   * File install can be debugged with the following command arguments:
//      /update
//   * Once the initial files have been copied, another instance is launched
//     from the installed location. To continue debugging, use the following
//     command arguments:
//      /ug
//  * Update check for apps that need it:
//      /ua
//  * Legacy hand-off install (Occurs from the machine install location)
//   * First /UI /lang en legacy_manifest_path
//   * This then launches a worker with:
//      /handoff "appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&appname=YouTube Uploader&needsadmin=False" /lang en  // NOLINT
//  * Core:
//      /c
//  * Cod Red check:
//      /cr
//  * Cod Red repair:
//   * Determining whether to elevate and non-elevated file install can be
//     debugged with the following command arguments:
//      /recover [/machine]
//   * Once the initial files have been copied, another instance is launched
//     from the installed location. To continue debugging, use the following
//     command arguments:
//      /ug [/machine]
//  * OneClick:
//      /pi "http://www.google.com/" "/install%20%22appguid=%7B8A69D345-D564-463C-AFF1-A69D9E530F96%7D%26lang=en%26appname=Google%2520Chrome%26needsadmin=false" /installsource oneclick  // NOLINT

#include "omaha/goopdate/goopdate.h"

#include <atlstr.h>
#include <new>
#include "base/scoped_ptr.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/google_update_recovery.h"
#include "omaha/common/logging.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/proc_utils.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/system_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/core/core.h"
#include "omaha/core/crash_handler.h"
#include "omaha/goopdate/browser_launcher.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/crash.h"
#include "omaha/goopdate/goopdate_helper.h"
#include "omaha/goopdate/google_update.h"
#include "omaha/goopdate/goopdate_metrics.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/goopdate_xml_parser.h"
#include "omaha/goopdate/goopdate-internal.h"
#include "omaha/goopdate/stats_uploader.h"
#include "omaha/goopdate/webplugin_utils.h"
#include "omaha/net/net_diags.h"
#include "omaha/net/browser_request.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"
#include "omaha/net/bits_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/recovery/repair_exe/repair_goopdate.h"
#include "omaha/service/service_main.h"
#include "omaha/setup/setup.h"
#include "omaha/setup/setup_service.h"
#include "omaha/worker/app_request_data.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/product_data.h"
#include "omaha/worker/worker.h"
#include "omaha/worker/worker_com_wrapper.h"
#include "omaha/worker/worker_event_logger.h"
#include "third_party/breakpad/src/client/windows/sender/crash_report_sender.h"
#include "third_party/breakpad/src/client/windows/handler/exception_handler.h"

// Generated by MIDL into $OBJ_ROOT/goopdate.
#include "goopdate/google_update_idl.h"

namespace omaha {

namespace {

#if DEBUG
const TCHAR* const kBuildType = _T("dbg");
#else
const TCHAR* const kBuildType = _T("opt");
#endif

#if OFFICIAL_BUILD
const TCHAR* const kOfficialBuild = _T("official");
#else
const TCHAR* const kOfficialBuild = _T("dev");
#endif

}  // namespace

namespace detail {

class GoopdateImpl {
 public:
  GoopdateImpl(Goopdate* goopdate, bool is_local_system);
  ~GoopdateImpl();

  HRESULT Main(HINSTANCE instance, const TCHAR* cmd_line, int cmd_show);

  bool is_local_system() const { return is_local_system_; }

  CommandLineArgs args() const { return args_; }
  CString cmd_line() const { return cmd_line_; }

 private:
  HRESULT DoMain(HINSTANCE instance, const TCHAR* cmd_line, int cmd_show);

  // Returns whether a process is a machine process.
  // Does not determine whether the process has the appropriate privileges.
  bool IsMachineProcess();

  HRESULT LoadResourceDllIfNecessary(CommandLineMode mode,
                                     const CString& resource_dir);

  HRESULT SetUsageStatsEnable();

  // Handles error conditions by showing UI.
  HRESULT HandleError(HRESULT hr);

  // Handles response to /pi command.
  HRESULT HandleWebPlugin();

  // Handles responses to /cr command.
  HRESULT HandleCodeRedCheck();

  // Handles /ui command.
  HRESULT HandleLegacyUI();

  // Handles /report command.
  HRESULT HandleReportCrash();

  // Handles /uiuser command.
  HRESULT HandleLegacyManifestHandoff();

  // Download Callback for Code Red.
  static HRESULT CodeRedDownloadCallback(const TCHAR* url,
                                         const TCHAR* file_path,
                                         void*);
  HRESULT DoWorker();

  // Generates a divide by zero to trigger breakpad dump.
  // Is only enabled in debug builds.
  HRESULT DoCrash();

  // Register a product with Goopdate and install Goopdate.
  HRESULT DoRegisterProduct();

  // Does the work for DoRegisterProduct().
  HRESULT DoRegisterProductHelper(bool is_machine,
                                  AppManager* app_manager,
                                  AppData* app_data);

  // Unregister a product from Goopdate.
  HRESULT DoUnregisterProduct();

  // Setup phase1 for self update.
  HRESULT DoSelfUpdate();

  // Setup phase2 for self update.
  HRESULT DoCompleteSelfUpdate();

  // Handles the recover command in Google Update.
  HRESULT DoRecover();

  // Uninstalls Google Update if a /install process failed to install itself
  // or the app and there are no other apps registered.
  HRESULT UninstallIfNecessary();

  // Called by operator new or operator new[] when they cannot satisfy
  // a request for additional storage.
  static void OutOfMemoryHandler();

  HINSTANCE module_instance_;  // Current module instance.
  CString cmd_line_;           // Command line, as provided by the OS.
  int cmd_show_;

  CommandLineArgs args_;       // Command line options and flags.

  // True if the process belongs to a machine Omaha "session".
  bool is_machine_;
  bool is_local_system_;       // True if running as LOCAL_SYSTEM.
  CString this_version_;       // Version of this Goopdate DLL.

  // True if Omaha has been uninstalled by the Worker.
  bool has_uninstalled_;

  // Language identifier for the current user locale.
  CString user_default_language_id_;

  scoped_ptr<ResourceManager> resource_manager_;
  Goopdate* goopdate_;

  DISALLOW_EVIL_CONSTRUCTORS(GoopdateImpl);
};

GoopdateImpl::GoopdateImpl(Goopdate* goopdate, bool is_local_system)
    : module_instance_(NULL),
      cmd_show_(0),
      is_machine_(false),
      is_local_system_(is_local_system),
      has_uninstalled_(false),
      goopdate_(goopdate) {
  ASSERT1(goopdate);
  // The command line needs to be parsed to accurately determine if the
  // current process is a machine process or not. Use the value of
  // is_local_system until that.
  is_machine_ = is_local_system_;

  // Install an error-handling mechanism which gets called when new operator
  // fails to allocate memory.
  VERIFY1(set_new_handler(&GoopdateImpl::OutOfMemoryHandler) == 0);

  // Install the exception handler.
  VERIFY1(SUCCEEDED(Crash::InstallCrashHandler(is_machine_)));

  // Initialize the global metrics collection.
  stats_report::g_global_metrics.Initialize();
}

GoopdateImpl::~GoopdateImpl() {
  CORE_LOG(L2, (_T("[GoopdateImpl::~GoopdateImpl]")));

  // Bug 994348 does not repro anymore.
  // If the assert fires, clean up the key, and fix the code if we have unit
  // tests or application code that create the key.
  ASSERT(!RegKey::HasKey(_T("HKEY_USERS\\.DEFAULT\\Software\\Google\\Update")),
         (_T("This assert has fired because it has found the registry key at ")
          _T("'HKEY_USERS\\.DEFAULT\\Software\\Google\\Update'. ")
          _T("Please delete the key and report to omaha-core team if ")
          _T("the assert fires again.")));

  // The global metrics collection must be uninitialized before the metrics
  // destructors are called.
  stats_report::g_global_metrics.Uninitialize();

  // Uninstall the exception handler. Program crashes are handled by Windows
  // Error Reporting (WER) beyond this point.
  Crash::UninstallCrashHandler();

  // Reset the new handler.
  set_new_handler(NULL);

  // This check must be the last thing before exiting the process.
  if (COMMANDLINE_MODE_INSTALL == args_.mode && args_.is_oem_set ||
      ConfigManager::Instance()->IsOemInstalling(is_machine_)) {
    // During an OEM install, there should be no persistent IDs.
    ASSERT1(!RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueMachineId));
    ASSERT1(!RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueUserId));

    ASSERT1(RegKey::HasValue(MACHINE_REG_UPDATE, kRegValueOemInstallTimeSec) ||
            !is_machine_ ||
            !vista_util::IsUserAdmin() ||
            !ConfigManager::Instance()->IsWindowsInstalling());
  }
}

HRESULT GoopdateImpl::HandleError(HRESULT hr) {
  // TODO(Omaha): An error dialog should ideally be shown for all
  // errors that Main encounters. This will require a bit of work to
  // ensure that we do not end up showing multiple dialogs, that we
  // have resources to show a localized error message, that we show
  // a message correctly even if we have not parsed the command line,
  // or encounter an error in command line parsing, etc.

#pragma warning(push)
// C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled by
// a case label.
#pragma warning(disable : 4061)
  switch (args_.mode) {
    case COMMANDLINE_MODE_HANDOFF_INSTALL:
    case COMMANDLINE_MODE_IG:
    case COMMANDLINE_MODE_INSTALL: {
      CString primary_app_name;
      ASSERT1(!args_.extra.apps.empty());
      if (!args_.extra.apps.empty()) {
        primary_app_name = args_.extra.apps[0].app_name;
      }

      CString error_text;
      switch (hr) {
        // TODO(omaha): It would be nice if we could display this in the full
        // UI so we can provide a link to the Help Center.
        case OMAHA_NET_E_WINHTTP_NOT_AVAILABLE:
          error_text.FormatMessage(IDS_WINDOWS_IS_NOT_UP_TO_DATE,
                                   primary_app_name);
          break;
        default:
          error_text.FormatMessage(IDS_SETUP_FAILED, hr);
          break;
      }

      if (!args_.is_silent_set) {
        goopdate_utils::DisplayErrorInMessageBox(error_text, primary_app_name);
      }
      return S_OK;
    }

    default:
      return E_NOTIMPL;
  }
#pragma warning(pop)
}

HRESULT GoopdateImpl::Main(HINSTANCE instance,
                           const TCHAR* cmd_line,
                           int cmd_show) {
  HRESULT hr = DoMain(instance, cmd_line, cmd_show);

  CORE_LOG(L2, (_T("[has_uninstalled is %d]"), has_uninstalled_));

  // For install processes, verify the Google Update EULA has been accepted and
  // we can use the network unless a) the command line specifies EULA is
  // required or b) in OEM installing mode, which also prevents network use.
  if ((COMMANDLINE_MODE_INSTALL == args_.mode ||
       COMMANDLINE_MODE_IG == args_.mode ||
       COMMANDLINE_MODE_HANDOFF_INSTALL == args_.mode) &&
       SUCCEEDED(hr)) {
    ASSERT1(ConfigManager::Instance()->CanUseNetwork(is_machine_) ||
            ConfigManager::Instance()->IsOemInstalling(is_machine_) ||
            args_.is_eula_required_set);
  }

  // In the /install case, clean up if Google Update and/or app install did not
  // complete successfully.
  // Only aggregate the metrics if there is no chance that Google Update has
  // or may be uninstalled and the process has the appropriate permissions.
  // Uninstall will aggregate and report the metrics as appropriate.
  bool did_install_uninstall_fail = false;
  if (COMMANDLINE_MODE_INSTALL == args_.mode) {
    did_install_uninstall_fail = FAILED(UninstallIfNecessary());
  } else if (!has_uninstalled_) {
    if (args_.mode == COMMANDLINE_MODE_UA) {
      VERIFY1(SUCCEEDED(AggregateAndReportMetrics(is_machine_, false)));
    } else if (!is_machine_ || vista_util::IsUserAdmin()) {
      VERIFY1(SUCCEEDED(AggregateMetrics(is_machine_)));
    }
  }

  // Uninitializing the network configuration must happen after reporting the
  // metrics. The call succeeds even if the network has not been initialized
  // due to errors up the execution path.
  NetworkConfig::DeleteInstance();

  if (COMMANDLINE_MODE_INSTALL == args_.mode &&
      args_.is_oem_set &&
      SUCCEEDED(hr) &&
      !ConfigManager::Instance()->IsOemInstalling(is_machine_)) {
    ASSERT1(false);
    hr = GOOPDATE_E_OEM_INSTALL_SUCCEEDED_BUT_NOT_IN_OEM_INSTALLING_MODE;
  }

  // Verify that Google Update is either completely installed or uninstalled.
  // Do not check in the following cases:
  // * Modes that may exit during Setup, during uninstall, or while Omaha
  //   is partially installed.
  // * The mode is unknown, which means the args were not be parsed.
  // * /cr instance, which may exit after Omaha is uninstalled.
  // * /install instance that would not have called Setup.Uninstall().
  // * /install instance when Uninstall failed for some reason since the
  //   the consistency check may expect the wrong state.
  if (COMMANDLINE_MODE_REGSERVER != args_.mode &&
      COMMANDLINE_MODE_UNREGSERVER != args_.mode &&
      COMMANDLINE_MODE_COMSERVER != args_.mode &&
      COMMANDLINE_MODE_CODE_RED_CHECK != args_.mode &&
      COMMANDLINE_MODE_SERVICE_REGISTER != args_.mode &&
      COMMANDLINE_MODE_SERVICE_UNREGISTER != args_.mode &&
      COMMANDLINE_MODE_UNKNOWN != args_.mode &&
      !(COMMANDLINE_MODE_INSTALL == args_.mode &&
        is_machine_ &&
        !vista_util::IsUserAdmin()) &&
      !did_install_uninstall_fail) {
    Setup::CheckInstallStateConsistency(is_machine_);
  }

  return hr;
}

HRESULT GoopdateImpl::DoMain(HINSTANCE instance,
                             const TCHAR* cmd_line,
                             int cmd_show) {
  module_instance_ = instance;
  cmd_line_ = cmd_line;
  cmd_show_ = cmd_show;

  // The system terminates the process without displaying a retry dialog box
  // for the user. GoogleUpdate has no user state to be saved, therefore
  // prompting the user is meaningless.
  VERIFY1(SUCCEEDED(SetProcessSilentShutdown()));

  int major_version(0);
  int minor_version(0);
  int service_pack_major(0);
  int service_pack_minor(0);
  TCHAR name[MAX_PATH] = {0};
  VERIFY1(SystemInfo::GetSystemVersion(&major_version,
                                       &minor_version,
                                       &service_pack_major,
                                       &service_pack_minor,
                                       name,
                                       arraysize(name)));
  metric_windows_major_version    = major_version;
  metric_windows_minor_version    = minor_version;
  metric_windows_sp_major_version = service_pack_major;
  metric_windows_sp_minor_version = service_pack_minor;

  InitializeVersionFromModule(module_instance_);
  this_version_ = GetVersionString();

  TCHAR path[MAX_PATH] = {0};
  VERIFY1(::GetModuleFileName(instance, path, MAX_PATH));
  OPT_LOG(L1, (_T("[%s][version %s][%s][%s]"),
               path, this_version_, kBuildType, kOfficialBuild));

  CORE_LOG(L2,
      (_T("[is system %d][elevated admin %d][non-elevated admin %d]"),
       is_local_system_,
       vista_util::IsUserAdmin(),
       vista_util::IsUserNonElevatedAdmin()));

  HRESULT hr = omaha::ParseCommandLine(cmd_line, &args_);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Parse cmd line failed][0x%08x]"), hr));
    return hr;
  }

  // IsMachineProcess requires the command line be parsed first.
  is_machine_ = IsMachineProcess();
  CORE_LOG(L2, (_T("[is machine %d]"), is_machine_));

  // After parsing the command line, reinstall the crash handler to match the
  // state of the process.
  if (is_machine_ != Crash::is_machine()) {
    VERIFY1(SUCCEEDED(Crash::InstallCrashHandler(is_machine_)));
  }

  // Set the current directory to be the one that the DLL was launched from.
  TCHAR module_directory[MAX_PATH] = {0};
  if (!GetModuleDirectory(instance, module_directory)) {
     return HRESULTFromLastError();
  }
  if (!::SetCurrentDirectory(module_directory)) {
    return HRESULTFromLastError();
  }
  OPT_LOG(L3, (_T("[Current dir][%s]"), module_directory));

  // Set the usage stats as soon as possible, which is after the command line
  // has been parsed, so that we can report crashes and other stats.
  VERIFY1(SUCCEEDED(SetUsageStatsEnable()));

  VERIFY1(SUCCEEDED(internal::PromoteAppEulaAccepted(is_machine_)));

  hr = LoadResourceDllIfNecessary(args_.mode, module_directory);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[LoadResourceDllIfNecessary failed][0x%08x]"), hr));
    return hr;
  }

  user_default_language_id_ = ResourceManager::GetDefaultUserLanguage();

#pragma warning(push)
// C4061: enumerator 'xxx' in switch of enum 'yyy' is not explicitly handled by
// a case label.
#pragma warning(disable : 4061)
  // Save the mode on the stack for post-mortem debugging purposes.
  volatile CommandLineMode mode = args_.mode;
  switch (args_.mode) {
    // Delegate to the service or the core. Both have reliability requirements
    // and resource constraints. Generally speaking, they do not use COM nor
    // networking code.
    case COMMANDLINE_MODE_SERVICE:
      return omaha::ServiceModule().Main(cmd_show);

    case COMMANDLINE_MODE_SERVICE_REGISTER:
      return SetupService::InstallService(app_util::GetModulePath(NULL));

    case COMMANDLINE_MODE_SERVICE_UNREGISTER:
      return SetupService::UninstallService();

    default: {
      scoped_co_init init_com_apt(COINIT_MULTITHREADED);
      hr = init_com_apt.hresult();
      if (FAILED(hr)) {
        return hr;
      }
      hr = ::CoInitializeSecurity(
          NULL,
          -1,
          NULL,   // Let COM choose what authentication services to register.
          NULL,
          RPC_C_AUTHN_LEVEL_PKT_PRIVACY,  // Data integrity and encryption.
          RPC_C_IMP_LEVEL_IDENTIFY,       // Only allow a server to identify.
          NULL,
          EOAC_DYNAMIC_CLOAKING,
          NULL);
      if (FAILED(hr)) {
        return hr;
      }

      switch (args_.mode) {
        case COMMANDLINE_MODE_CORE:
          return omaha::Core().Main(is_local_system_,
                                    !args_.is_crash_handler_disabled);

        case COMMANDLINE_MODE_CRASH_HANDLER:
          return omaha::CrashHandler().Main(is_local_system_);

        case COMMANDLINE_MODE_NOARGS:
          // TODO(omaha): Supported only for legacy Compatibility. Error out
          // when legacy handoff support is removed as Omaha should not be
          // called without arguments otherwise.
          return S_OK;

        case COMMANDLINE_MODE_LEGACYUI:
          return HandleLegacyUI();

        case COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF:
          return HandleLegacyManifestHandoff();

        case COMMANDLINE_MODE_UNREGISTER_PRODUCT:
          return DoUnregisterProduct();

        default: {
          hr = goopdate_utils::ConfigureNetwork(
              is_machine_ && vista_util::IsUserAdmin(),
              is_local_system_);
          if (FAILED(hr)) {
            VERIFY1(SUCCEEDED(HandleError(hr)));
            return hr;
          }

          switch (args_.mode) {
            case COMMANDLINE_MODE_WEBPLUGIN:
              return HandleWebPlugin();

            case COMMANDLINE_MODE_CODE_RED_CHECK:
              return HandleCodeRedCheck();

            case COMMANDLINE_MODE_NETDIAGS:
              return NetDiags().Main();

            case COMMANDLINE_MODE_REGISTER_PRODUCT:
              return DoRegisterProduct();

            case COMMANDLINE_MODE_UPDATE:
              return DoSelfUpdate();

            case COMMANDLINE_MODE_UG:
              return DoCompleteSelfUpdate();

            case COMMANDLINE_MODE_RECOVER:
              return DoRecover();

            case COMMANDLINE_MODE_HANDOFF_INSTALL:
            case COMMANDLINE_MODE_IG:
            case COMMANDLINE_MODE_INSTALL:
            case COMMANDLINE_MODE_UA:
              return DoWorker();
            case COMMANDLINE_MODE_CRASH:
              return DoCrash();

            case COMMANDLINE_MODE_REPORTCRASH:
              return HandleReportCrash();

            case COMMANDLINE_MODE_REGSERVER:
            case COMMANDLINE_MODE_UNREGSERVER:
            case COMMANDLINE_MODE_COMSERVER:
              return omaha::GoogleUpdate().Main();

            default:
              // We have a COMMANDLINE_MODE_ that isn't being handled.
              ASSERT1(false);
              OPT_LOG(LE, (_T("[Command line has unhandled mode]")));
              return E_UNEXPECTED;
          }
        }
      }
    }
  }
#pragma warning(pop)
}

bool GoopdateImpl::IsMachineProcess() {
  Tristate needs_admin(TRISTATE_NONE);
  if (!args_.extra.apps.empty()) {
    needs_admin = args_.extra.apps[0].needs_admin ? TRISTATE_TRUE :
                                                    TRISTATE_FALSE;
  }

  return goopdate_utils::IsMachineProcess(
      args_.mode,
      goopdate_utils::IsRunningFromOfficialGoopdateDir(true),
      is_local_system_,
      args_.is_machine_set,
      needs_admin);
}


HRESULT GoopdateImpl::HandleReportCrash() {
  // Catch exceptions to avoid reporting crashes when handling a crash.
  // TODO(omaha): maybe let Windows handle the crashes when reporting crashes
  // in certain interactive modes.
  HRESULT hr = S_OK;
  __try {
    // Crashes are uploaded always in the out-of-process case. This is handled
    // in Crash::Report().
    // Google Update crashes are uploaded only for for the users that have opted
    // in, when network use is allowed, and from systems that are not
    // development or test.
    // All GoogleUpdate crashes are logged in the Windows event log for
    // applications, unless the logging is disabled by the administrator.
    const bool can_upload_in_process =
        ConfigManager::Instance()->CanCollectStats(is_machine_) &&
        ConfigManager::Instance()->CanUseNetwork(is_machine_) &&
        !goopdate_utils::IsTestSource();
    hr = Crash::Report(can_upload_in_process,
                       args_.crash_filename,
                       args_.custom_info_filename,
                       user_default_language_id_);
  }
  __except(EXCEPTION_EXECUTE_HANDLER) {
    hr = E_FAIL;
  }
  return hr;
}

HRESULT GoopdateImpl::HandleLegacyManifestHandoff() {
  // TODO(omaha): Once the core supports metric aggregation, move this metric
  // to LegacyManifestHandler::HandleEvent().
  ++metric_handoff_legacy_user;

  // The user core launches the is_legacy_user_manifest_cmd worker to
  // process the manifest file that is dropped into the initial manifest
  // directory by the omaha1 installers.
  if (!goopdate_utils::IsRunningFromOfficialGoopdateDir(false)) {
    // TODO(omaha): If the /UI process fails the legacy installer will
    // not display anything to the user. We might have to launch UI here.
    ASSERT1(false);
    return E_FAIL;
  }

  return goopdate_utils::HandleLegacyManifestHandoff(
      args_.legacy_manifest_path,
      false);
}

HRESULT GoopdateImpl::HandleLegacyUI() {
  ++metric_handoff_legacy_machine;

  // A omaha1 installer does a handoff install using
  // the /UI switch. The format of the command line is as follows:
  // /UI /lang <lang> manfest_filename or /UI legacy_manifest_path
  // The legacy installer should have elevated and we should
  // be running from the program files directory since this command can
  // only be passed to a registered, machine omaha.
  if (!vista_util::IsUserAdmin() ||
      !goopdate_utils::IsRunningFromOfficialGoopdateDir(true)) {
    // TODO(omaha): If the /UI process fails the legacy installer will
    // not display anything to the user. We might have to launch UI here.
    ASSERT1(false);
    return E_FAIL;
  }

  // We ignore the /lang switch that is passed on the cmd line,
  // since we get the language from the manifest file and pass that to the
  // worker.
  return goopdate_utils::HandleLegacyManifestHandoff(
      args_.legacy_manifest_path, true);
}

// The resource dll is loaded only in the following cases:
// 1. Initial setup: /install
// 2. Google Update and app install: /ig
// 3. Handoff install: /handoff
// 4. App update worker: /ua
// 5. Self-Update: /ug
HRESULT GoopdateImpl::LoadResourceDllIfNecessary(CommandLineMode mode,
                                                 const CString& resource_dir) {
  switch (mode) {
    case COMMANDLINE_MODE_INSTALL:          // has UI on errors
    case COMMANDLINE_MODE_IG:               // has UI
    case COMMANDLINE_MODE_HANDOFF_INSTALL:  // has UI
    case COMMANDLINE_MODE_UG:               // TODO(omaha): Why is it loaded?
    case COMMANDLINE_MODE_UA:               // Worker, etc. load strings.
    case COMMANDLINE_MODE_COMSERVER:        // Returns strings to caller.
    case COMMANDLINE_MODE_SERVICE_REGISTER:
    case COMMANDLINE_MODE_SERVICE_UNREGISTER:
      // Load the resource DLL for these modes.
      break;
    case COMMANDLINE_MODE_UNKNOWN:
    case COMMANDLINE_MODE_NOARGS:
    case COMMANDLINE_MODE_CORE:
    case COMMANDLINE_MODE_CRASH_HANDLER:
    case COMMANDLINE_MODE_SERVICE:
    case COMMANDLINE_MODE_REGSERVER:
    case COMMANDLINE_MODE_UNREGSERVER:
    case COMMANDLINE_MODE_NETDIAGS:
    case COMMANDLINE_MODE_CRASH:
    case COMMANDLINE_MODE_REPORTCRASH:
    case COMMANDLINE_MODE_UPDATE:
    case COMMANDLINE_MODE_RECOVER:
    case COMMANDLINE_MODE_WEBPLUGIN:
    case COMMANDLINE_MODE_CODE_RED_CHECK:
    case COMMANDLINE_MODE_LEGACYUI:
    case COMMANDLINE_MODE_LEGACY_MANIFEST_HANDOFF:
    case COMMANDLINE_MODE_REGISTER_PRODUCT:
    case COMMANDLINE_MODE_UNREGISTER_PRODUCT:
    default:
      // Thse modes do not need the resource DLL.
      return S_OK;
  }

  resource_manager_.reset(new ResourceManager(is_machine_, resource_dir));

  HRESULT hr = resource_manager_->LoadResourceDll(args_.extra.language);
  if (FAILED(hr)) {
    ++metric_load_resource_dll_failed;
    ASSERT(false, (_T("LoadResourceDll failed with 0x%08x"), hr));
  }
  return hr;
}

// Writes the information for the primary app to enable Omaha to send usage
// stats now. It will be set for each app in the args when they are installed.
HRESULT GoopdateImpl::SetUsageStatsEnable() {
  if (COMMANDLINE_MODE_UPDATE == args_.mode) {
    VERIFY1(SUCCEEDED(
        goopdate_utils::ConvertLegacyUsageStats(is_machine_)));
  }

  if (args_.extra.apps.empty()) {
    return S_OK;
  }

  HRESULT hr = goopdate_utils::SetUsageStatsEnable(
      args_.extra.apps[0].needs_admin,
      GuidToString(args_.extra.apps[0].app_guid),
      args_.extra.usage_stats_enable);
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[SetUsageStatsEnable failed][0x%08x]"), hr));

    if ((HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) == hr) &&
        args_.extra.apps[0].needs_admin &&
        !vista_util::IsUserAdmin()) {
      CORE_LOG(L3, (_T("[Process does not have permission to HKLM]")));
      return S_OK;
    }
    return hr;
  }

  return S_OK;
}

HRESULT GoopdateImpl::HandleCodeRedCheck() {
  CORE_LOG(L2, (_T("[GoopdateImpl::HandleCodeRedCheck]")));
  ++metric_cr_process_total;

  // Call the utils method instead of member method because we want to execute
  // as little code as possible.
  bool is_machine = goopdate_utils::IsMachineProcess(args_.mode,
                                                     false,   // machine dir
                                                     is_local_system_,
                                                     args_.is_machine_set,
                                                     TRISTATE_NONE);
  ASSERT1(IsMachineProcess() == is_machine);

  if (!ConfigManager::Instance()->CanUseNetwork(is_machine)) {
    CORE_LOG(L1,
             (_T("[Code Red check not sent because network use prohibited]")));
    return GOOPDATE_E_CANNOT_USE_NETWORK;
  }

  AppManager app_manager(is_machine);
  ProductData product_data;

  HRESULT hr = FixGoogleUpdate(kGoogleUpdateAppId,
                               this_version_,
                               _T(""),     // Goopdate doesn't have a language.
                               is_machine,
                               &GoopdateImpl::CodeRedDownloadCallback,
                               NULL);
  CORE_LOG(L2, (_T("[FixGoogleUpdate returned 0x%08x]"), hr));
  return S_OK;
}

// Returns S_OK when the download of the Code Red file succeeds and E_FAIL
// otherwise.
HRESULT GoopdateImpl::CodeRedDownloadCallback(const TCHAR* url,
                                              const TCHAR* file_path,
                                              void*) {
  ++metric_cr_callback_total;

  NetworkConfig& network_config = NetworkConfig::Instance();
  NetworkRequest network_request(network_config.session());

  network_request.AddHttpRequest(new SimpleRequest);

  // BITS takes the job to BG_JOB_STATE_TRANSIENT_ERROR when the server returns
  // 204. After the "no progress time out", the BITS job errors out. Since
  // BITS follows the WinHTTP in the fallback chain, the code is expected to
  // execute only if WinHTTP fails to get a response from the server.

  // Assumes the caller is not logged on if the function failed.
  // BITS transfers files only when the job owner is logged on.
  bool is_logged_on(false);
  HRESULT hr = IsUserLoggedOn(&is_logged_on);
  ASSERT1(SUCCEEDED(hr) || !is_logged_on);
  if (is_logged_on) {
    BitsRequest* bits_request(new BitsRequest);
    bits_request->set_minimum_retry_delay(60);
    bits_request->set_no_progress_timeout(15);
    network_request.AddHttpRequest(bits_request);
  }

  network_request.AddHttpRequest(new BrowserRequest);
  hr = network_request.DownloadFile(CString(url), CString(file_path));
  if (FAILED(hr)) {
    return E_FAIL;
  }

  switch (network_request.http_status_code()) {
    case HTTP_STATUS_OK:
      ++metric_cr_callback_status_200;
      return S_OK;
    case HTTP_STATUS_NO_CONTENT:
      ++metric_cr_callback_status_204;
      return E_FAIL;
    default:
      ++metric_cr_callback_status_other;
      return E_FAIL;
  }
}

// Until bug 1135173 is fixed:
// Running a OneClick cross-install at the same time as Setup has undefined
// behavior. If Setup wins the race to the lock, it may delete the files that
// the cross-install wants to copy. Also, since the plugin and /plugin instance
// do not check the Setup Lock, they may try to invoke a version that is being
// installed, removed, or rolled back.

// If we're called with the /webplugin command, we need to handle it and exit.
// This is called from the browser and the command line arguments come from the
// website so we need to be restrictive of what we let past. If everything from
// the plugin is valid, we'll relaunch goopdate with the proper commands.
HRESULT GoopdateImpl::HandleWebPlugin() {
  return webplugin_utils::DoOneClickInstall(args_);
}

HRESULT GoopdateImpl::DoCompleteSelfUpdate() {
  OPT_LOG(L1, (_T("[GoopdateImpl::DoCompleteSelfUpdate]")));
  // TODO(omaha): The use of is_machine_set is very non-intuituve.
  // It is used only for machine repair situations when elevation is required
  // in vista or in case of XP. For all other cases this is not used and plain
  // /update is used to invoke recovery.
  // Another consequence of this is that in some cases we run
  // the full Omaha update logic i.e. ShouldInstall etc for recovery.
  // Consider making recovery not go through all this code.
  OPT_LOG(L1, (_T("[self update][is_machine=%d]"), is_machine_));

  Ping ping;
  return FinishGoogleUpdateInstall(args_,
                                   is_machine_,
                                   true,
                                   &ping,
                                   NULL);
}

// Calls DoRegisterProductHelper and sends a ping with the result.
// TODO(omaha): Use a more common code flow with normal installs or remove
// registerproduct altogether when redesigning Omaha.
HRESULT GoopdateImpl::DoRegisterProduct() {
  OPT_LOG(L1, (_T("[GoopdateImpl::DoRegisterProduct]")));

  ASSERT1(!goopdate_utils::IsRunningFromOfficialGoopdateDir(false) &&
          !goopdate_utils::IsRunningFromOfficialGoopdateDir(true));

  ASSERT1(!args_.extra.apps.empty());
  // TODO(omaha): Support bundles. Only supports one app currently.
  ASSERT1(1 == args_.extra.apps.size());
  bool extra_needs_admin = args_.extra.apps[0].needs_admin;

  AppManager app_manager(extra_needs_admin);
  ProductDataVector products;
  app_manager.ConvertCommandLineToProductData(args_, &products);
  AppData app_data = products[0].app_data();

  HRESULT hr = DoRegisterProductHelper(extra_needs_admin,
                                       &app_manager,
                                       &app_data);

  const PingEvent::Results event_result = SUCCEEDED(hr) ?
                                          PingEvent::EVENT_RESULT_SUCCESS :
                                          PingEvent::EVENT_RESULT_ERROR;

  AppRequestData app_request_data(app_data);
  PingEvent ping_event(PingEvent::EVENT_REGISTER_PRODUCT_COMPLETE,
                       event_result,
                       hr,  // error code
                       0,  // extra code 1
                       app_data.previous_version());
  app_request_data.AddPingEvent(ping_event);
  AppRequest app_request(app_request_data);
  Request request(extra_needs_admin);
  request.AddAppRequest(app_request);

  Ping ping;
  ping.SendPing(&request);

  return hr;
}

HRESULT GoopdateImpl::DoRegisterProductHelper(bool is_machine,
                                              AppManager* app_manager,
                                              AppData* app_data) {
  ASSERT1(app_data);
  ASSERT1(app_manager);
  // Ensure we're running as elevated admin if needs_admin is true.
  if (is_machine && !vista_util::IsUserAdmin()) {
    CORE_LOG(LE, (_T("[DoRegisterProduct][needs admin & user not admin]")));
    return GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP;
  }

  // Get product GUID from args and register it in CLIENTS.
  HRESULT hr = app_manager->RegisterProduct(args_.extra.apps[0].app_guid,
                                            args_.extra.apps[0].app_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[AppManager::RegisterProduct failed][0x%08x]"), hr));
    return hr;
  }

  Setup setup(is_machine, &args_);
  hr = setup.InstallSelfSilently();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Setup::InstallSelfSilently failed][0x%08x]"), hr));
    return hr;
  }

  // Populate the app's ClientState key.
  VERIFY1(SUCCEEDED(app_manager->WritePreInstallData(*app_data)));
  VERIFY1(SUCCEEDED(app_manager->InitializeApplicationState(app_data)));

  return S_OK;
}

HRESULT GoopdateImpl::DoUnregisterProduct() {
  bool extra_needs_admin = args_.extra.apps[0].needs_admin;

  // Ensure we're running as elevated admin if needs_admin is true.
  if (extra_needs_admin && !vista_util::IsUserAdmin()) {
    CORE_LOG(LE, (_T("[DoUnregisterProduct][needs admin & user not admin]")));
    return GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP;
  }

  AppManager app_manager(extra_needs_admin);
  HRESULT hr = app_manager.UnregisterProduct(args_.extra.apps[0].app_guid);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[AppManager::UnregisterProduct failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT GoopdateImpl::DoSelfUpdate() {
  OPT_LOG(L1, (_T("[GoopdateImpl::DoSelfUpdate]")));

  Setup setup(is_machine_, &args_);
  HRESULT hr = setup.UpdateSelfSilently();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[Setup::UpdateSelfSilently failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

// Attempts to launch the repair file elevated using the MSP. If elevation is
// not needed or fails, attempts to install without elevating.
// The code_red_metainstaller_path arg is the repair file.
HRESULT GoopdateImpl::DoRecover() {
  OPT_LOG(L1, (_T("[GoopdateImpl::DoRecover()]")));

  CommandLineBuilder builder(COMMANDLINE_MODE_UPDATE);

  HRESULT hr = S_OK;
  if (LaunchRepairFileElevated(is_machine_,
                               args_.code_red_metainstaller_path,
                               builder.GetCommandLineArgs(),
                               &hr)) {
    ASSERT1(SUCCEEDED(hr));
    return S_OK;
  }

  if (FAILED(hr)) {
    OPT_LOG(LW, (_T("[LaunchRepairFileElevated failed][0x%08x]"), hr));
  }

  Setup setup(is_machine_, &args_);
  hr = setup.RepairSilently();
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Non-elevated repair failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

// Clears the EULA flag in the handoff instance in case an older installer that
// does not know about the EULA flag is used to launch the install.
// Failing to clear flag fails installation because this would prevent updates.
HRESULT GoopdateImpl::DoWorker() {
  if (COMMANDLINE_MODE_HANDOFF_INSTALL == args_.mode &&
      !args_.is_eula_required_set) {
    HRESULT hr = Setup::SetEulaAccepted(is_machine_);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[Setup::SetEulaAccepted failed][0x%08x]"), hr));
      return hr;
    }
  }

  omaha::Worker worker(is_machine_);
  HRESULT hr = worker.Main(goopdate_);
  has_uninstalled_ = worker.has_uninstalled();

  // The UA worker always returns S_OK. UA can be launched by the scheduled task
  // or the core, neither of which wait for a return code. On Vista, returning
  // an error code from the scheduled task reportedly causes issues with the
  // task scheduler in rare cases, so returning S_OK helps with that as well.
  if (args_.mode == COMMANDLINE_MODE_UA) {
    return S_OK;
  }

  return hr;
}

HRESULT GoopdateImpl::DoCrash() {
#if DEBUG
  return static_cast<HRESULT>(Crash::CrashNow());
#else
  return S_OK;
#endif
}

// In dbg builds, also checks the post conditions of a /install process to
// ensure that the registry is correctly populated or has been cleaned up.
HRESULT GoopdateImpl::UninstallIfNecessary() {
  ASSERT1(COMMANDLINE_MODE_INSTALL == args_.mode);
  ASSERT(!has_uninstalled_, (_T("Worker doesn't uninstall in /install mode")));

  if (is_machine_ && !vista_util::IsUserAdmin()) {
    // The non-elevated instance, so do not try to uninstall.
    ASSERT1(!args_.is_install_elevated);
    return S_OK;
  } else {
    CORE_LOG(L2, (_T("[GoopdateImpl::Main /install][Uninstall if necessary]")));
    // COM must be initialized in order to uninstall the scheduled task(s).
    scoped_co_init init_com_apt(COINIT_MULTITHREADED);
    Setup setup(is_machine_, &args_);
    return setup.Uninstall();
  }
}

void GoopdateImpl::OutOfMemoryHandler() {
  ::RaiseException(EXCEPTION_ACCESS_VIOLATION,
                   EXCEPTION_NONCONTINUABLE,
                   0,
                   NULL);
}

}  // namespace detail

namespace internal {

// Returns early if Google Update's EULA is already accepted, as indicated by
// the lack of eulaaccepted in the Update key.
// The apps' values are not modified or deleted.
HRESULT PromoteAppEulaAccepted(bool is_machine) {
  const TCHAR* update_key_name =
      ConfigManager::Instance()->registry_update(is_machine);
  if (!RegKey::HasValue(update_key_name, kRegValueOmahaEulaAccepted)) {
    return S_OK;
  }

  const TCHAR* state_key_name =
      ConfigManager::Instance()->registry_client_state(is_machine);

  RegKey state_key;
  HRESULT hr = state_key.Open(state_key_name, KEY_READ);
  if (FAILED(hr)) {
    if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr) {
      return S_FALSE;
    }
    return hr;
  }

  // TODO(omaha): This should actually be iterating over registered products
  // rather than present ClientState keys. These are identical in most cases.
  int num_sub_keys = state_key.GetSubkeyCount();
  for (int i = 0; i < num_sub_keys; ++i) {
    CString sub_key_name;
    if (FAILED(state_key.GetSubkeyNameAt(i, &sub_key_name))) {
      continue;
    }

    if (goopdate_utils::IsAppEulaAccepted(is_machine, sub_key_name, true)) {
      ASSERT1(kGoogleUpdateAppId != sub_key_name);
      return Setup::SetEulaAccepted(is_machine);
    }
  }

  return S_OK;
}

}  // namespace internal

Goopdate::Goopdate(bool is_local_system) {
  CORE_LOG(L2, (_T("[Goopdate::Goopdate]")));
  impl_.reset(new detail::GoopdateImpl(this, is_local_system));
}

Goopdate::~Goopdate() {
  CORE_LOG(L2, (_T("[Goopdate::~Goopdate]")));
}

HRESULT Goopdate::Main(HINSTANCE instance,
                       const TCHAR* cmd_line,
                       int cmd_show) {
  return impl_->Main(instance, cmd_line, cmd_show);
}

bool Goopdate::is_local_system() const {
  return impl_->is_local_system();
}

CommandLineArgs Goopdate::args() const {
  return impl_->args();
}

CString Goopdate::cmd_line() const {
  return impl_->cmd_line();
}

OBJECT_ENTRY_AUTO(__uuidof(ProcessLauncherClass), ProcessLauncher)
OBJECT_ENTRY_AUTO(__uuidof(OnDemandUserAppsClass), OnDemandCOMClass)
OBJECT_ENTRY_AUTO(__uuidof(OnDemandMachineAppsClass), OnDemandCOMClassMachine)

}  // namespace omaha
