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

#include "omaha/common/command_line_builder.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(CommandLineBuilder, BuildUnknown) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UNKNOWN);
  ExpectAsserts expect_asserts;
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildNoArgs) {
  CommandLineBuilder builder(COMMANDLINE_MODE_NOARGS);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildCore) {
  CommandLineBuilder builder(COMMANDLINE_MODE_CORE);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/c"), cmd_line);
}

TEST(CommandLineBuilder, BuildService) {
  CommandLineBuilder builder(COMMANDLINE_MODE_SERVICE);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/svc"), cmd_line);
}

TEST(CommandLineBuilder, BuildMediumService) {
  CommandLineBuilder builder(COMMANDLINE_MODE_MEDIUM_SERVICE);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/medsvc"), cmd_line);
}

TEST(CommandLineBuilder, BuildRegServer) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REGSERVER);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/regserver"), cmd_line);
}

TEST(CommandLineBuilder, BuildUnregServer) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UNREGSERVER);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/unregserver"), cmd_line);
}

TEST(CommandLineBuilder, BuildCrashNoFilename) {
  CommandLineBuilder builder(COMMANDLINE_MODE_CRASH);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/crash"), cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrash) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  ExpectAsserts expect_asserts;  // Missing filename.
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashWithFilename) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("foo.dmp"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/report \"foo.dmp\""), cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashWithFilenameMachine) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("foo.dmp"));
  builder.set_is_machine_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/report \"foo.dmp\" /machine"), cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashWithEnclosedFilename) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("\"foo.dmp\""));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/report \"foo.dmp\""), cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashWithCustomInfo) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  ExpectAsserts expect_asserts;  // Missing filename.
  builder.set_custom_info_filename(_T("foo.txt"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashWithFileanameWithCustomInfo) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("foo.dmp"));
  builder.set_custom_info_filename(_T("foo.txt"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/report \"foo.dmp\" /custom_info_filename \"foo.txt\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashWithFileanameWithCustomInfoMachine) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("foo.dmp"));
  builder.set_custom_info_filename(_T("foo.txt"));
  builder.set_is_machine_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(
      _T("/report \"foo.dmp\" /machine /custom_info_filename \"foo.txt\""),
      cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashWithEnclosedFileanameWithCustomInfo) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("\"foo.dmp\""));
  builder.set_custom_info_filename(_T("\"foo.txt\""));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/report \"foo.dmp\" /custom_info_filename \"foo.txt\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashInteractiveWithFilename) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("foo.dmp"));
  builder.set_is_interactive_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/report /i \"foo.dmp\""), cmd_line);
}

TEST(CommandLineBuilder, BuildReportCrashMachineInteractiveWithFilename) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REPORTCRASH);
  builder.set_crash_filename(_T("foo.dmp"));
  builder.set_is_machine_set(true);
  builder.set_is_interactive_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/report /i \"foo.dmp\" /machine"), cmd_line);
}

TEST(CommandLineBuilder, BuildInstall) {
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  ExpectAsserts expect_asserts;  // Missing parameters.
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildInstallWithExtraArgs) {
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/install \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildInstallWithExtraArgsSilent) {
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_is_silent_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/install \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\" /silent"),
               cmd_line);
}

TEST(CommandLineBuilder, BuildInstallWithExtraArgsSilentAndAlwaysLaunchCmd) {
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_is_silent_set(true);
  builder.set_is_always_launch_cmd_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/install \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\" /silent ")
               _T("/alwayslaunchcmd"),
               cmd_line);
}

TEST(CommandLineBuilder, BuildInstallWithExtraArgsSessionId) {
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_session_id(_T("{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/install \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /sessionid \"{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildInstallWithExtraArgsEnterprise) {
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_is_enterprise_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/install \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /enterprise"),
               cmd_line);
}

TEST(CommandLineBuilder, BuildUpdate) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UPDATE);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/update"), cmd_line);
}

TEST(CommandLineBuilder, BuildUpdateWithSessionId) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UPDATE);
  builder.set_session_id(_T("{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(
      _T("/update /sessionid \"{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}\""),
      cmd_line);
}

// The /update builder works when not used with GoogleUpdate.exe.
TEST(CommandLineBuilder, BuildUpdateAndGetCommandLineWithNonGoogleUpdateExe) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UPDATE);
  CString cmd_line = builder.GetCommandLine(_T("C:\\") MAIN_EXE_BASE_NAME _T("Setup_en.exe"));
  EXPECT_STREQ(_T("\"C:\\") MAIN_EXE_BASE_NAME _T("Setup_en.exe\" /update"), cmd_line);
}

// The /update builder should not be used with GoogleUpdate.exe directly.
TEST(CommandLineBuilder, BuildUpdateAndGetCommandLineWithGoogleUpdateExe) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UPDATE);
  ExpectAsserts expect_asserts;
  CString cmd_line = builder.GetCommandLine(_T("C:\\") MAIN_EXE_BASE_NAME _T(".exe"));
  EXPECT_STREQ(_T("\"C:\\") MAIN_EXE_BASE_NAME _T(".exe\" /update"), cmd_line);
}

TEST(CommandLineBuilder, BuildComServer) {
  CommandLineBuilder builder(COMMANDLINE_MODE_COMSERVER);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("-Embedding"), cmd_line);
}

TEST(CommandLineBuilder, BuildComBroker) {
  CommandLineBuilder builder(COMMANDLINE_MODE_COMBROKER);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/broker"), cmd_line);
}

TEST(CommandLineBuilder, BuildOnDemand) {
  CommandLineBuilder builder(COMMANDLINE_MODE_ONDEMAND);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/ondemand"), cmd_line);
}

TEST(CommandLineBuilder, BuildCodeRedCheck) {
  CommandLineBuilder builder(COMMANDLINE_MODE_CODE_RED_CHECK);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/cr"), cmd_line);
}

TEST(CommandLineBuilder, BuildRecover) {
  CommandLineBuilder builder(COMMANDLINE_MODE_RECOVER);
  ExpectAsserts expect_asserts;
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildRecoverWithMIPath) {
  CommandLineBuilder builder(COMMANDLINE_MODE_RECOVER);
  builder.set_code_red_metainstaller_path(_T("foo.exe"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/recover foo.exe"), cmd_line);
}

TEST(CommandLineBuilder, BuildUA) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UA);
  ExpectAsserts expect_asserts;
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildUAWithInstallSource) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UA);
  builder.set_install_source(_T("blah"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/ua /installsource blah"), cmd_line);
}

TEST(CommandLineBuilder, BuildHandoffInstall) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  ExpectAsserts expect_asserts;  // Missing extra args.
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T(""), cmd_line);
}

TEST(CommandLineBuilder, BuildHandoffInstallWithExtraArgs) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildHandoffInstallWithExtraArgsSessionId) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_session_id(_T("{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /sessionid \"{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}\""),
               cmd_line);
}

TEST(CommandLineBuilder, SetOfflineDirName_AbsoluteDirNoGUID) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);

  EXPECT_FAILED(
      builder.SetOfflineDirName(_T("c:\\offline_dir")));
}

TEST(CommandLineBuilder, SetOfflineDirName_AbsoluteDirGUIDTrailingBackslash) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);

  EXPECT_FAILED(builder.SetOfflineDirName(
      _T("c:\\offline_dir\\{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\\")));
}

TEST(CommandLineBuilder, SetOfflineDirName_AbsoluteDirGUID) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);

  EXPECT_SUCCEEDED(builder.SetOfflineDirName(
      _T("c:\\offline_dir\\{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}")));
  EXPECT_STREQ(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}"),
               builder.offline_dir_name());
}

TEST(CommandLineBuilder, SetOfflineDirNameGUID) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);

  EXPECT_SUCCEEDED(
      builder.SetOfflineDirName(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}")));
  EXPECT_STREQ(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}"),
               builder.offline_dir_name());
}

TEST(CommandLineBuilder, BuildHandoffInstallWithExtraArgsOffline) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_install_source(_T("offline"));
  EXPECT_SUCCEEDED(
      builder.SetOfflineDirName(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}")));

  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /installsource offline")
               _T(" /offlinedir \"{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildHandoffInstallWithExtraArgsSilentOffline) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_install_source(_T("offline"));
  builder.set_is_silent_set(true);
  EXPECT_SUCCEEDED(
      builder.SetOfflineDirName(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}")));

  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /installsource offline")
               _T(" /silent")
               _T(" /offlinedir \"{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\""),
               cmd_line);
}

TEST(CommandLineBuilder, 
     BuildHandoffInstallWithExtraArgsSilentAlwaysLaunchCmd) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_install_source(_T("offline"));
  builder.set_is_silent_set(true);
  builder.set_is_always_launch_cmd_set(true);
  EXPECT_SUCCEEDED(
      builder.SetOfflineDirName(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}")));

  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /installsource offline")
               _T(" /silent /alwayslaunchcmd")
               _T(" /offlinedir \"{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildHandoffWithAppArgsSilentOffline) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_app_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                       _T("installerdata=foobar%45"));
  builder.set_install_source(_T("offline"));
  builder.set_is_silent_set(true);
  EXPECT_SUCCEEDED(
      builder.SetOfflineDirName(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}")));

  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /appargs \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("installerdata=foobar%45\"")
               _T(" /installsource offline")
               _T(" /silent")
               _T(" /offlinedir \"{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildHandoffWithAppArgsSilentOfflineEulaRequired) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_app_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                       _T("installerdata=foobar%45"));
  builder.set_install_source(_T("offline"));
  builder.set_is_silent_set(true);
  builder.set_is_eula_required_set(true);
  EXPECT_SUCCEEDED(
      builder.SetOfflineDirName(_T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}")));

  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /appargs \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("installerdata=foobar%45\"")
               _T(" /installsource offline")
               _T(" /silent /eularequired")
               _T(" /offlinedir \"{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildHandoffInstallWithExtraArgsEnterprise) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HANDOFF_INSTALL);
  builder.set_extra_args(_T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                         _T("appname=YouTubeUploader&needsadmin=False&")
                         _T("lang=en"));
  builder.set_is_enterprise_set(true);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/handoff \"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
               _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
               _T(" /enterprise"),
               cmd_line);
}

TEST(CommandLineBuilder, BuildRegisterProduct) {
  CommandLineBuilder builder(COMMANDLINE_MODE_REGISTER_PRODUCT);
  builder.set_extra_args(_T("appguid={7DD3DAE3-87F1-4CFE-8BF4-452C74421401}&")
                         _T("appname=Google Toolbar&needsadmin=True&")
                         _T("lang=en"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/registerproduct ")
               _T("\"appguid={7DD3DAE3-87F1-4CFE-8BF4-452C74421401}&")
               _T("appname=Google Toolbar&needsadmin=True&lang=en\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildUnregisterProduc) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UNREGISTER_PRODUCT);
  builder.set_extra_args(_T("appguid={7DD3DAE3-87F1-4CFE-8BF4-452C74421401}&")
                         _T("needsadmin=True&lang=en"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/unregisterproduct ")
               _T("\"appguid={7DD3DAE3-87F1-4CFE-8BF4-452C74421401}&")
               _T("needsadmin=True&lang=en\""),
               cmd_line);
}

TEST(CommandLineBuilder, BuildUninstall) {
  CommandLineBuilder builder(COMMANDLINE_MODE_UNINSTALL);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/uninstall"), cmd_line);
}

TEST(CommandLineBuilder, BuildPing) {
  CommandLineBuilder builder(COMMANDLINE_MODE_PING);
  builder.set_ping_string(_T("foo"));
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/ping foo"), cmd_line);
}

TEST(CommandLineBuilder, BuildHealthCheck) {
  CommandLineBuilder builder(COMMANDLINE_MODE_HEALTH_CHECK);
  CString cmd_line = builder.GetCommandLineArgs();
  EXPECT_STREQ(_T("/healthcheck"), cmd_line);
}

}  // namespace omaha

