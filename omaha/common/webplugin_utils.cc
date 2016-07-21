// Copyright 2008-2009 Google Inc.
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

#include "omaha/common/webplugin_utils.h"

#include "omaha/base/app_util.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/lang.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"

namespace omaha {

namespace webplugin_utils {

HRESULT SanitizeExtraArgs(const CString& extra_args_in,
                          CString* extra_args_out) {
  ASSERT1(extra_args_out);

  HRESULT hr = StringEscape(extra_args_in, true, extra_args_out);
  if (FAILED(hr)) {
    return hr;
  }

  // Now we unescape a selective white-list of characters.
  extra_args_out->Replace(_T("%3D"), _T("="));
  extra_args_out->Replace(_T("%26"), _T("&"));
  extra_args_out->Replace(_T("%7B"), _T("{"));
  extra_args_out->Replace(_T("%7D"), _T("}"));
  extra_args_out->Replace(_T("%25"), _T("%"));

  return hr;
}

HRESULT BuildWebPluginCommandLine(const CString& url_domain,
                                  const CString& extra_args,
                                  CString* final_cmd_line_args) {
  ASSERT1(!extra_args.IsEmpty());
  ASSERT1(final_cmd_line_args);

  CORE_LOG(L2, (_T("[BuildWebPluginCommandLine][%s][%s]"),
                url_domain, extra_args));

  CString extra_args_sanitized;
  HRESULT hr = webplugin_utils::SanitizeExtraArgs(extra_args,
                                                  &extra_args_sanitized);
  if (FAILED(hr)) {
    return hr;
  }

  CommandLineBuilder install_builder(COMMANDLINE_MODE_INSTALL);
  install_builder.set_extra_args(extra_args_sanitized);
  CString cmd_line_args(install_builder.GetCommandLineArgs());

  CString url_domain_encoded;
  CString cmd_line_args_encoded;
  hr = StringEscape(url_domain, true, &url_domain_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  hr = StringEscape(cmd_line_args, true, &cmd_line_args_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  CommandLineBuilder webplugin_builder(COMMANDLINE_MODE_WEBPLUGIN);
  webplugin_builder.set_webplugin_url_domain(url_domain_encoded);
  webplugin_builder.set_webplugin_args(cmd_line_args_encoded);
  webplugin_builder.set_install_source(kCmdLineInstallSource_OneClick);
  CString webplugin_cmd_line(webplugin_builder.GetCommandLineArgs());

  CString cmd_line_web_plugin;
  SafeCStringFormat(&cmd_line_web_plugin, _T("/%s"), kCmdLineWebPlugin);

  if (!String_StartsWith(webplugin_cmd_line, cmd_line_web_plugin, false)) {
    return E_UNEXPECTED;
  }

  *final_cmd_line_args =
      webplugin_cmd_line.Mid(cmd_line_web_plugin.GetLength() + 1);

  CORE_LOG(L2, (_T("[BuildWebPluginCommandLine][%s]"), *final_cmd_line_args));
  return S_OK;
}

HRESULT IsLanguageSupported(const CString& webplugin_args) {
  CString cmd_line;
  SafeCStringFormat(&cmd_line, _T("gu.exe %s"), webplugin_args);
  CommandLineArgs parsed_args;
  HRESULT hr = ParseCommandLine(cmd_line, &parsed_args);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ParseCommandLine failed][0x%08x]"), hr));
    return hr;
  }


  if (!lang::IsLanguageSupported(parsed_args.extra.language)) {
    CORE_LOG(LE, (_T("Language not supported][%s]"),
                  parsed_args.extra.language));
    return GOOPDATE_E_ONECLICK_LANGUAGE_NOT_SUPPORTED;
  }

  return S_OK;
}

HRESULT BuildOneClickWorkerArgs(const CommandLineArgs& args,
                                CString* oneclick_args) {
  ASSERT1(oneclick_args);

  // Since this is being called via WebPlugin only, we can rebuild the
  // command line arguments from the valid params we can send on.
  // For example, the web plugin will not send crash_cmd or debug_cmd
  // or reg_server or unreg_server so we don't have to worry about those here.
  CString cmd_line_args;
  CommandLineArgs webplugin_cmdline_args;

  // ParseCommandLine assumes the first argument is the program being run.
  // Don't want to enforce that constraint on our callers, so we prepend with a
  // fake exe name.
  CString args_to_parse;
  SafeCStringFormat(&args_to_parse, _T("%s %s"),
                    kOmahaShellFileName,
                    args.webplugin_args);

  // Parse the arguments we received as the second parameter to /webplugin.
  HRESULT hr = ParseCommandLine(args_to_parse, &webplugin_cmdline_args);
  if (FAILED(hr)) {
    return hr;
  }

  // Silent and other non-standard installs could be malicious. Prevent them.
  if (webplugin_cmdline_args.mode != COMMANDLINE_MODE_INSTALL) {
    return E_INVALIDARG;
  }
  if (webplugin_cmdline_args.is_silent_set ||
      webplugin_cmdline_args.is_eula_required_set) {
    return E_INVALIDARG;
  }

  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(webplugin_cmdline_args.extra_args_str);

  // We expect this value from the plugin.
  ASSERT1(!args.install_source.IsEmpty());
  if (args.install_source.IsEmpty()) {
    return E_INVALIDARG;
  }

  builder.set_install_source(args.install_source);

  *oneclick_args = builder.GetCommandLineArgs();

  return S_OK;
}

// It is important that current_goopdate_path be the version path and not the
// Update\ path.
HRESULT CopyGoopdateToTempDir(const CPath& current_goopdate_path,
                              CPath* goopdate_temp_path) {
  ASSERT1(goopdate_temp_path);

  // Create a unique directory in the user's temp directory.
  GUID guid = GUID_NULL;
  HRESULT hr = ::CoCreateGuid(&guid);
  if (FAILED(hr)) {
    return hr;
  }
  CString guid_str = GuidToString(guid);
  ASSERT1(!guid_str.IsEmpty());

  CString temp_dir = app_util::GetTempDir();
  ASSERT1(!temp_dir.IsEmpty());

  CPath temp_path = temp_dir.GetString();
  temp_path.Append(guid_str);
  temp_path.Canonicalize();

  hr = CreateDir(temp_path, NULL);
  if (FAILED(hr)) {
    return hr;
  }

  hr = File::CopyTree(current_goopdate_path, temp_path, true);
  if (FAILED(hr)) {
    return hr;
  }

  CORE_LOG(L2, (_T("[CopyGoopdateToTempDir][temp_path = %s]"), temp_path));
  *goopdate_temp_path = temp_path;
  return S_OK;
}

HRESULT DoOneClickInstall(const CommandLineArgs& args) {
  CString cmd_line_args;
  HRESULT hr = BuildOneClickWorkerArgs(args, &cmd_line_args);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[BuildOneClickWorkerArgs failed][0x%08x]"), hr));
    return hr;
  }

  CORE_LOG(L2, (_T("[DoOneClickInstall][cmd_line_args: %s]"), cmd_line_args));

  // Check if we're running from the machine dir.
  // If we're not, we must be running from user directory since OneClick only
  // works against installed versions of Omaha.
  CPath current_goopdate_path(app_util::GetCurrentModuleDirectory());
  CPath goopdate_temp_path;
  hr = CopyGoopdateToTempDir(current_goopdate_path, &goopdate_temp_path);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CopyGoopdateToTempDir failed][0x%08x]"), hr));
    return hr;
  }

  CPath goopdate_temp_exe_path = goopdate_temp_path;
  goopdate_temp_exe_path.Append(kOmahaShellFileName);

  // Launch goopdate again with the updated command line arguments.
  hr = System::ShellExecuteProcess(goopdate_temp_exe_path,
                                   cmd_line_args,
                                   NULL,
                                   NULL);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ShellExecuteProcess failed][%s][0x%08x]"),
                  goopdate_temp_exe_path, hr));
    return hr;
  }

  return S_OK;
}

}  // namespace webplugin_utils

}  // namespace omaha
