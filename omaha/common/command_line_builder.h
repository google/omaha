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

#ifndef OMAHA_COMMON_COMMAND_LINE_BUILDER_H__
#define OMAHA_COMMON_COMMAND_LINE_BUILDER_H__

#include <windows.h>
#include <atlstr.h>

#include "base/basictypes.h"
#include "omaha/common/command_line.h"

namespace omaha {

// This class builds a GoogleUpdate.exe command line and makes sure it's
// valid against the GoopdateCommandLineValidator.
class CommandLineBuilder {
 public:
  explicit CommandLineBuilder(CommandLineMode mode);
  ~CommandLineBuilder();

  CommandLineMode mode() const { return mode_; }

  bool is_interactive_set() const { return is_interactive_set_; }
  void set_is_interactive_set(bool is_interactive_set);

  bool is_machine_set() const { return is_machine_set_; }
  void set_is_machine_set(bool is_machine_set);

  bool is_silent_set() const { return is_silent_set_; }
  void set_is_silent_set(bool is_silent_set);

  bool is_always_launch_cmd_set() const { return is_always_launch_cmd_set_; }
  void set_is_always_launch_cmd_set(bool is_always_launch_cmd_set);

  bool is_eula_required_set() const { return is_eula_required_set_; }
  void set_is_eula_required_set(bool is_eula_required_set);

  bool is_enterprise_set() const { return is_enterprise_set_; }
  void set_is_enterprise_set(bool is_enterprise_set);

  CString extra_args() const { return extra_args_; }
  void set_extra_args(const CString& extra_args);

  CString app_args() const { return app_args_; }
  void set_app_args(const CString& app_args);

  CString install_source() const { return install_source_; }
  void set_install_source(const CString& install_source);

  CString session_id() const { return session_id_; }
  void set_session_id(const CString& session_id);

  CString crash_filename() const { return crash_filename_; }
  void set_crash_filename(const CString& crash_filename);

  CString custom_info_filename() const { return custom_info_filename_; }
  void set_custom_info_filename(const CString& custom_info_filename);

  CString code_red_metainstaller_path() const {
    return code_red_metainstaller_path_;
  }
  void set_code_red_metainstaller_path(
      const CString& code_red_metainstaller_path);

  CString ping_string() const { return ping_string_; }
  void set_ping_string(const CString& ping_string);

  CString offline_dir_name() const { return offline_dir_name_; }

  // The offline directory name is a string GUID. This function sets the
  // offline directory name. It takes as input an offline_dir that is either a
  // GUID or ends with a GUID. For instance:
  // "{GUID}" and "C:\Path\{GUID}" are both valid inputs.
  // In the latter case, "C:\Path\" is stripped off to get "{GUID}".
  HRESULT SetOfflineDirName(const CString& offline_dir);

  // Outputs the proper command line string for the properties that are set.
  // If the properties aren't in a valid combination, function will assert.
  CString GetCommandLineArgs()  const;

  CString GetCommandLine(const CString& program_name) const;

 private:
  CString GetSingleSwitch(const CString& switch_name) const;
  CString GetExtraAndAppArgs(const TCHAR* extra_switch_name) const;

  CString GetCore() const;
  CString GetCrashHandler() const;
  CString GetService() const;
  CString GetServiceRegister() const;
  CString GetServiceUnregister() const;
  CString GetRegServer() const;
  CString GetUnregServer() const;
  CString GetCrash() const;
  CString GetReportCrash() const;
  CString GetInstall() const;
  CString GetUpdate() const;
  CString GetHandoffInstall() const;
  CString GetUA() const;
  CString GetRecover() const;
  CString GetCodeRedCheck() const;
  CString GetComServer() const;
  CString GetComBroker() const;
  CString GetOnDemand() const;
  CString GetMediumService() const;
  CString GetUninstall() const;
  CString GetRegisterProduct() const;
  CString GetUnregisterProduct() const;
  CString GetPing() const;
  CString GetHealthCheck() const;
  CString GetRegisterMsiHelper() const;

  const CommandLineMode mode_;
  bool is_interactive_set_;
  bool is_machine_set_;
  bool is_silent_set_;
  bool is_always_launch_cmd_set_;
  bool is_eula_required_set_;
  bool is_enterprise_set_;
  CString extra_args_;
  CString app_args_;
  CString install_source_;
  CString crash_filename_;
  CString custom_info_filename_;
  CString code_red_metainstaller_path_;
  CString ping_string_;
  CString offline_dir_name_;
  CString session_id_;

  DISALLOW_COPY_AND_ASSIGN(CommandLineBuilder);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_COMMAND_LINE_BUILDER_H__

