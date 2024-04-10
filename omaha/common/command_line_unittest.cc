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

#include "omaha/base/utils.h"
#include "omaha/common/command_line.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

// Used by extra_args_parser_unittest.cc.
void VerifyCommandLineExtraArgs(const CommandLineExtraArgs& expected_val,
                                const CommandLineExtraArgs& actual_val);

namespace {

#define YOUTUBEUPLOADEREN_TAG_WITHOUT_QUOTES \
    _T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&") \
    _T("appname=YouTubeUploader&needsadmin=False&lang=en")

#define YOUTUBEUPLOADEREN_APP_ARGS_WITHOUT_QUOTES \
    _T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&") \
    _T("installerdata=YouTube%20Uploader%20Data")

#define YOUTUBEUPLOADEREN_TAG \
    _T("\"") YOUTUBEUPLOADEREN_TAG_WITHOUT_QUOTES _T("\"")

#define YOUTUBEUPLOADEREN_APP_ARGS \
    _T("\"") YOUTUBEUPLOADEREN_APP_ARGS_WITHOUT_QUOTES _T("\"")

void VerifyCommandLineArgs(const CommandLineArgs& expected,
                           const CommandLineArgs& actual) {
  EXPECT_EQ(expected.mode, actual.mode);

  EXPECT_EQ(expected.is_interactive_set, actual.is_interactive_set);
  EXPECT_EQ(expected.is_machine_set, actual.is_machine_set);
  EXPECT_EQ(expected.is_install_elevated, actual.is_install_elevated);
  EXPECT_EQ(expected.is_silent_set, actual.is_silent_set);
  EXPECT_EQ(expected.is_always_launch_cmd_set, actual.is_always_launch_cmd_set);
  EXPECT_EQ(expected.is_eula_required_set, actual.is_eula_required_set);
  EXPECT_EQ(expected.is_enterprise_set, actual.is_enterprise_set);
  EXPECT_EQ(expected.is_offline_set, actual.is_offline_set);
  EXPECT_EQ(expected.is_oem_set, actual.is_oem_set);

  EXPECT_STREQ(expected.extra_args_str, actual.extra_args_str);
  EXPECT_STREQ(expected.app_args_str, actual.app_args_str);
  EXPECT_STREQ(expected.install_source, actual.install_source);
  EXPECT_STREQ(expected.crash_filename, actual.crash_filename);
  EXPECT_STREQ(expected.custom_info_filename, actual.custom_info_filename);
  EXPECT_STREQ(expected.legacy_manifest_path, actual.legacy_manifest_path);
  EXPECT_STREQ(expected.code_red_metainstaller_path,
               actual.code_red_metainstaller_path);
  EXPECT_STREQ(expected.offline_dir_name, actual.offline_dir_name);

  VerifyCommandLineExtraArgs(expected.extra, actual.extra);
}

void VerifyArgsWithSingleYouTubeUploaderEnApp(
    const CommandLineArgs& expected_without_app,
    const CommandLineArgs& actual,
    bool expect_app_args,
    bool expect_language) {
  CommandLineArgs expected(expected_without_app);

  const GUID expected_guid = {0xA4F7B07B, 0xB9BD, 0x4A33,
                              {0xB1, 0x36, 0x96, 0xD2, 0xAD, 0xFB, 0x60, 0xCB}};
  CommandLineAppArgs app_args;
  app_args.app_guid = expected_guid;
  app_args.app_name = _T("YouTubeUploader");
  app_args.needs_admin = NEEDS_ADMIN_NO;
  if (expected.extra_args_str.IsEmpty()) {
    expected.extra_args_str = YOUTUBEUPLOADEREN_TAG_WITHOUT_QUOTES;
  }
  if (expect_language) {
    expected.extra.language = _T("en");
  }
  if (expect_app_args) {
    expected.app_args_str =
        YOUTUBEUPLOADEREN_APP_ARGS_WITHOUT_QUOTES;
    app_args.encoded_installer_data = _T("YouTube%20Uploader%20Data");
  }

  expected.extra.apps.push_back(app_args);
  expected.extra.bundle_name = _T("YouTubeUploader");
  VerifyCommandLineArgs(expected, actual);
}

}  // namespace

void VerifyCommandLineExtraArgs(const CommandLineExtraArgs& expected_val,
                                const CommandLineExtraArgs& actual_val) {
  EXPECT_STREQ(expected_val.bundle_name, actual_val.bundle_name);
  EXPECT_STREQ(GuidToString(expected_val.installation_id),
               GuidToString(actual_val.installation_id));
  EXPECT_STREQ(expected_val.brand_code, actual_val.brand_code);
  EXPECT_STREQ(expected_val.client_id, actual_val.client_id);
  EXPECT_STREQ(expected_val.experiment_labels,
               actual_val.experiment_labels);
  EXPECT_STREQ(expected_val.referral_id, actual_val.referral_id);
  EXPECT_STREQ(expected_val.language, actual_val.language);
#if defined(HAS_DEVICE_MANAGEMENT)
  EXPECT_STREQ(expected_val.enrollment_token, actual_val.enrollment_token);
#endif
  EXPECT_EQ(expected_val.browser_type, actual_val.browser_type);
  EXPECT_EQ(expected_val.usage_stats_enable, actual_val.usage_stats_enable);
  EXPECT_EQ(expected_val.runtime_mode, actual_val.runtime_mode);

  EXPECT_EQ(expected_val.apps.size(), actual_val.apps.size());

  for (size_t i = 0; i < actual_val.apps.size(); ++i) {
    CommandLineAppArgs expected = expected_val.apps[i];
    CommandLineAppArgs actual = actual_val.apps[i];

    EXPECT_STREQ(GuidToString(expected.app_guid),
                 GuidToString(actual.app_guid));
    EXPECT_STREQ(expected.app_name, actual.app_name);
    EXPECT_EQ(expected.needs_admin, actual.needs_admin);
    EXPECT_STREQ(expected.ap, actual.ap);
    EXPECT_STREQ(expected.tt_token, actual.tt_token);
    EXPECT_STREQ(expected.encoded_installer_data,
                 actual.encoded_installer_data);
    EXPECT_STREQ(expected.install_data_index, actual.install_data_index);
    EXPECT_STREQ(expected.experiment_labels, actual.experiment_labels);
  }
}

class CommandLineTest : public testing::Test {
 protected:
  CommandLineArgs args_;
  CommandLineArgs expected_;
};

TEST(CommandLineSimpleTest, GetCmdLineTail1) {
  EXPECT_STREQ(_T(""),  GetCmdLineTail(_T("")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail2) {
  EXPECT_STREQ(_T(""), GetCmdLineTail(_T("a")));
}
TEST(CommandLineSimpleTest, GetCmdLineTail3) {
  EXPECT_STREQ(_T(""), GetCmdLineTail(_T("goopdate.exe")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail4) {
  // Double quotes.
  EXPECT_STREQ(_T(""), GetCmdLineTail(_T("\"Google Update.exe\"")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail5) {
  // Argument.
  EXPECT_STREQ(_T("foobar"), GetCmdLineTail(_T("goopdate.exe foobar")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail6) {
  // Double quotes and argument.
  EXPECT_STREQ(_T("foobar"),
               GetCmdLineTail(_T("\"Google Update.exe\" foobar")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail7) {
  // Double quotes and inner double quote and argument.
  EXPECT_STREQ(_T("foobar"),
               GetCmdLineTail(_T("\"Google\"\" Update.exe\" foobar")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail8) {
  // Double quotes and two arguments.
  EXPECT_STREQ(_T("foo bar"),
               GetCmdLineTail(_T("\"Google Update.exe\" foo bar")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail9) {
  // Double quotes and one argument with quotes.
  EXPECT_STREQ(_T("\"foo bar\""),
               GetCmdLineTail(_T("\"Google Update.exe\" \"foo bar\"")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail10) {
  // \t as white space.
  EXPECT_STREQ(_T("foo bar"),
               GetCmdLineTail(_T("\"Google Update.exe\"\tfoo bar")));
}

TEST(CommandLineSimpleTest, GetCmdLineTail11) {
  // Trailing space.
  EXPECT_STREQ(_T("foo bar "),
               GetCmdLineTail(_T("\"Google Update.exe\" foo bar ")));
}

//
// This block contains the positive test cases for each of the command lines.
// If you add a new command line parameter permutation, add a test case here.
//

// TODO(omaha): Add some negative failure cases to the command lines (like
// /install without "extraargs").

// TODO(omaha): This is an Omaha1 back-compat issue.  Omaha2 should _never_
// call googleupdate.exe with no arguments.  So when we stop supporting Omaha1
// handoffs we should remove this support.
  // Parse empty command line.
TEST_F(CommandLineTest, ParseCommandLine_Empty) {
  const TCHAR* kCmdLine = _T("");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_NOARGS;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path>
// Remember that by convention the OS is passing us the program executable
// name as the first token in the command line and the parsing code skips that.
TEST_F(CommandLineTest, ParseCommandLine_ProgramNameOnly) {
  const TCHAR* kCmdLine = _T("goopdate.exe");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_NOARGS;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /svc
TEST_F(CommandLineTest, ParseCommandLine_Svc) {
  const TCHAR* kCmdLine = _T("\"C:\\Program Files\\Google\\Common\\Update\\")
                           _T("1.0.18.0\\goopdate.exe\" /svc");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_SERVICE;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /medsvc
TEST_F(CommandLineTest, ParseCommandLine_MedSvc) {
  const TCHAR* kCmdLine = _T("\"C:\\Program Files\\Google\\Common\\Update\\")
                           _T("1.0.18.0\\goopdate.exe\" /medsvc");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_MEDIUM_SERVICE;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> -Embedding. The -Embedding text is injected via COM.
TEST_F(CommandLineTest, ParseCommandLine_Server) {
  const TCHAR* kCmdLine = _T("goopdate.exe -Embedding");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_COMSERVER;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /broker. Used for the special COM broker mode.
TEST_F(CommandLineTest, ParseCommandLine_Broker) {
  const TCHAR* kCmdLine = _T("goopdate.exe /broker");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_COMBROKER;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /ondemand.
TEST_F(CommandLineTest, ParseCommandLine_OnDemand) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ondemand");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_ONDEMAND;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /install "extraargs"
// Also tests that bundle_name takes the first app's appname when no bundlename
// is specified.
TEST_F(CommandLineTest, ParseCommandLine_Install) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False&")
      _T("appguid={C7A9A2F5-C4F9-42d3-8A8B-55086A205468}&")
      _T("appname=TestApp&needsadmin=true&lang=en")
#if defined(HAS_DEVICE_MANAGEMENT)
      _T("&etoken=f6f767ba-8cfb-4d95-a26a-b3d714ddf1a2")
#endif
      _T("\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;

  expected_.extra_args_str = _T("appguid=")
                             _T("{A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                             _T("appname=YouTubeUploader&needsadmin=False&")
                             _T("appguid=")
                             _T("{C7A9A2F5-C4F9-42d3-8A8B-55086A205468}&")
                             _T("appname=TestApp&needsadmin=true&lang=en")
#if defined(HAS_DEVICE_MANAGEMENT)
                             _T("&etoken=f6f767ba-8cfb-4d95-a26a-b3d714ddf1a2")
#endif
                             _T("");
  CommandLineAppArgs app_args;
  const GUID expected_guid = {0xA4F7B07B, 0xB9BD, 0x4A33,
                              {0xB1, 0x36, 0x96, 0xD2, 0xAD, 0xFB, 0x60, 0xCB}};
  app_args.app_guid = expected_guid;
  app_args.app_name = _T("YouTubeUploader");
  app_args.needs_admin = NEEDS_ADMIN_NO;
  expected_.extra.apps.push_back(app_args);

  CommandLineAppArgs app_args1;
  app_args1.app_guid =
      StringToGuid(_T("{C7A9A2F5-C4F9-42d3-8A8B-55086A205468}"));
  app_args1.app_name = _T("TestApp");
  app_args1.needs_admin = NEEDS_ADMIN_YES;
  expected_.extra.apps.push_back(app_args1);

  expected_.extra.bundle_name = _T("YouTubeUploader");
  expected_.extra.language = _T("en");
#if defined(HAS_DEVICE_MANAGEMENT)
  expected_.extra.enrollment_token = _T("f6f767ba-8cfb-4d95-a26a-b3d714ddf1a2");
#endif

  VerifyCommandLineArgs(expected_, args_);

  EXPECT_STREQ(_T("YouTubeUploader"), args_.extra.bundle_name);
}

// Parse: <path> /install "bundlename=My%20Bundle&extraargs"
// Tests bundlename override of appname.
TEST_F(CommandLineTest, ParseCommandLine_Install_BundleName) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ")
      _T("\"bundlename=My%20Bundle&")
      _T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False&\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;

  expected_.extra_args_str = _T("bundlename=My%20Bundle&appguid=")
                             _T("{A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                             _T("appname=YouTubeUploader&needsadmin=False&");
  CommandLineAppArgs app_args;
  const GUID expected_guid = {0xA4F7B07B, 0xB9BD, 0x4A33,
                              {0xB1, 0x36, 0x96, 0xD2, 0xAD, 0xFB, 0x60, 0xCB}};
  app_args.app_guid = expected_guid;
  app_args.app_name = _T("YouTubeUploader");
  app_args.needs_admin = NEEDS_ADMIN_NO;
  expected_.extra.apps.push_back(app_args);

  expected_.extra.bundle_name = _T("My Bundle");

  VerifyCommandLineArgs(expected_, args_);

  EXPECT_STREQ(_T("My Bundle"), args_.extra.bundle_name);
}

// Parse: <path> /install "extraargs" /oem
TEST_F(CommandLineTest, ParseCommandLine_InstallWithOem) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /oem");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_oem_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /enterprise
TEST_F(CommandLineTest, ParseCommandLine_InstallWithEnterprise) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /enterprise");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_enterprise_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" [/oem
// This tests how we handle a switch with a bracket, which represents optional
// parameters in a rule, when it appears in an actual command line.
TEST_F(CommandLineTest, ParseCommandLine_InstallWithOemIgnored) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" [/oem");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_oem_set = false;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /appargs <appargs>
TEST_F(CommandLineTest, ParseCommandLine_InstallWithAppArgs) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS;
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /install "extraargs" /oem /appargs <appargs>
TEST_F(CommandLineTest, ParseCommandLine_InstallWithOemAppArgs) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /oem")
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS;
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_oem_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /install "extraargs" /appargs <appargs> /silent
TEST_F(CommandLineTest, ParseCommandLine_InstallWithAppArgsSilent) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
                          _T(" /silent");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_silent_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse <path> /install "extraargs" /appargs <appargs> /silent /alwayslaunchcmd
TEST_F(CommandLineTest,
       ParseCommandLine_InstallWithAppArgsSilentAlwaysLaunchCmd) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
                          _T(" /silent /alwayslaunchcmd");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_silent_set = true;
  expected_.is_always_launch_cmd_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /install "extraargs" /oem /appargs <appargs> /silent
TEST_F(CommandLineTest, ParseCommandLine_InstallWithOemAppArgsSilent) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /oem")
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
                          _T(" /silent");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_oem_set = true;
  expected_.is_silent_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse:
//  <path> /install "extraargs" /oem /appargs <appargs> /silent /eularequired
TEST_F(CommandLineTest,
       ParseCommandLine_InstallWithOemAppArgsSilentEulaRequired) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
      _T(" /oem")
      _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
      _T(" /silent")
      _T(" /eularequired");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_oem_set = true;
  expected_.is_silent_set = true;
  expected_.is_eula_required_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /install "extraargs" /eularequired
TEST_F(CommandLineTest, ParseCommandLine_InstallEulaRequired) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
      _T(" /eularequired");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_eula_required_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /oem /installsource taggedmi
TEST_F(CommandLineTest, ParseCommandLine_InstallWithOemAndSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /oem")
                          _T(" /installsource taggedmi");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_oem_set = true;
  expected_.install_source = _T("taggedmi");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /installsource taggedmi
TEST_F(CommandLineTest, ParseCommandLine_InstallWithSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installsource taggedmi");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.install_source = _T("taggedmi");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /silent
TEST_F(CommandLineTest, ParseCommandLine_InstallSilent) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /silent");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_silent_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /silent /oem
TEST_F(CommandLineTest, ParseCommandLine_InstallSilentWithOem) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /silent")
                          _T(" /oem");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_silent_set = true;
  expected_.is_oem_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /installsource taggedmi /silent
TEST_F(CommandLineTest, ParseCommandLine_InstallSilentWithSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installsource taggedmi")
                          _T(" /silent");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.install_source = _T("taggedmi");
  expected_.is_silent_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /installelevated
TEST_F(CommandLineTest, ParseCommandLine_InstallElevated) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installelevated");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_install_elevated = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /installelevated /installsource taggedmi
TEST_F(CommandLineTest, ParseCommandLine_InstallElevatedWithSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installelevated /installsource taggedmi");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.is_install_elevated = true;
  expected_.install_source = _T("taggedmi");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /install "extraargs" /sessionid "guid"
TEST_F(CommandLineTest, ParseCommandLine_InstallSessionId) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ") YOUTUBEUPLOADEREN_TAG
      _T(" /sessionid \"{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;
  expected_.session_id = _T("{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /ig "extraargs"
TEST_F(CommandLineTest, ParseCommandLine_LegacyOmaha2Ig) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ig ") YOUTUBEUPLOADEREN_TAG;
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /handoff "extraargs"
TEST_F(CommandLineTest, ParseCommandLine_Handoff) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG;
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /installsource "asd"
TEST_F(CommandLineTest, ParseCommandLine_HandoffWithSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installsource taggedmi");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.install_source = _T("taggedmi");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /installsource offline /offlineinstall
TEST_F(CommandLineTest, ParseCommandLine_HandoffWithSourceOffline) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installsource offline")
                          _T(" /offlineinstall");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.install_source = _T("offline");
  expected_.is_offline_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /offlinedir "{GUID}"
TEST_F(CommandLineTest, ParseCommandLine_OfflineDirGUID) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /offlinedir")
                          _T(" \"{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_offline_set = true;
  expected_.offline_dir_name = _T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /offlinedir "Z:\{GUID}"
TEST_F(CommandLineTest, ParseCommandLine_OfflineDirAbsolutePathEndsInGUID) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /offlinedir")
                          _T(" \"Z:\\{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_offline_set = true;
  expected_.offline_dir_name = _T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /offlinedir "c:\dir"
// Fails since only a GUID is allowed for the offline dir name.
TEST_F(CommandLineTest, ParseCommandLine_OfflineDirAbsolutePathNoGUID) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /offlinedir \"c:\\offline_dir\"");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /handoff "extraargs" /appargs <appargs>
//               /installsource offline /offlinedir "{GUID}"
TEST_F(CommandLineTest, ParseCommandLine_HandoffWithAppArgsSourceOffline) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
                          _T(" /installsource offline")
                          _T(" /offlinedir")
                          _T(" \"{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.install_source = _T("offline");
  expected_.is_offline_set = true;
  expected_.offline_dir_name = _T("{B851CC84-A5C4-4769-92C1-DC6B0BB368B4}");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /handoff "extraargs" /silent
TEST_F(CommandLineTest, ParseCommandLine_HandoffSilent) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /silent");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_silent_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /silent /alwayslaunchcmd
TEST_F(CommandLineTest, ParseCommandLine_HandoffSilentAlwaysLaunchCmd) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /silent /alwayslaunchcmd");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_silent_set = true;
  expected_.is_always_launch_cmd_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /enterprise
TEST_F(CommandLineTest, ParseCommandLine_HandoffEnterprise) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /enterprise");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_enterprise_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /silent /offlineinstall
TEST_F(CommandLineTest,
       ParseCommandLine_HandoffSilentOfflineWithoutInstallSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /silent")
                          _T(" /offlineinstall");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_silent_set = true;
  expected_.is_offline_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /installsource "asd" /silent
TEST_F(CommandLineTest, ParseCommandLine_HandoffSilentWithSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installsource taggedmi")
                          _T(" /silent");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.install_source = _T("taggedmi");
  expected_.is_silent_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse:
//   <path> /handoff "extraargs" /installsource offline /silent /offlineinstall
TEST_F(CommandLineTest, ParseCommandLine_HandoffSilentWithSourceOffline) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /installsource offline")
                          _T(" /silent")
                          _T(" /offlineinstall");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.install_source = _T("offline");
  expected_.is_silent_set = true;
  expected_.is_offline_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /appargs <appargs>
TEST_F(CommandLineTest, ParseCommandLine_HandoffWithAppArgs) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS;
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /handoff "extraargs" /appargs <appargs> /silent
TEST_F(CommandLineTest, ParseCommandLine_HandoffSilentWithAppArgs) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
                          _T(" /silent");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_silent_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /handoff "extraargs" /appargs <appargs>
//               /installsource offline /silent /offlineinstall
TEST_F(CommandLineTest,
       ParseCommandLine_HandoffSilentWithAppArgsSourceOffline) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
                          _T(" /installsource offline")
                          _T(" /silent")
                          _T(" /offlineinstall");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.install_source = _T("offline");
  expected_.is_silent_set = true;
  expected_.is_offline_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /handoff "extraargs" /appargs <appargs>
//               /installsource offline /silent /offlineinstall /eularequired
TEST_F(CommandLineTest,
       ParseCommandLine_HandoffSilentWithAppArgsSourceOfflineEulaRequired) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /appargs ") YOUTUBEUPLOADEREN_APP_ARGS
                          _T(" /installsource offline")
                          _T(" /silent")
                          _T(" /offlineinstall")
                          _T(" /eularequired");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.install_source = _T("offline");
  expected_.is_silent_set = true;
  expected_.is_offline_set = true;
  expected_.is_eula_required_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, true, true);
}

// Parse: <path> /handoff "extraargs" /eularequired
TEST_F(CommandLineTest, ParseCommandLine_HandoffEulaRequired) {
  const TCHAR* kCmdLine = _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
                          _T(" /eularequired");

  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.is_eula_required_set = true;
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /handoff "extraargs" /sessionid "guid"
TEST_F(CommandLineTest, ParseCommandLine_HandoffSessionId) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /handoff ") YOUTUBEUPLOADEREN_TAG
      _T(" /sessionid \"{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}\"");

  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  expected_.session_id = _T("{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, true);
}

// Parse: <path> /ug
TEST_F(CommandLineTest, ParseCommandLine_LegacyOmaha2Ug) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ug");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /ua
TEST_F(CommandLineTest, ParseCommandLine_UaNoInstallSource) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ua");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_UA;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /ua /installsource core
TEST_F(CommandLineTest, ParseCommandLine_Ua) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ua /installsource core");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_UA;
  expected_.install_source = _T("core");
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /update
TEST_F(CommandLineTest, ParseCommandLine_Update) {
  const TCHAR* kCmdLine = _T("goopdate.exe /update");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_UPDATE;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /update /sessionid "guid"
TEST_F(CommandLineTest, ParseCommandLine_UpdateSessionId) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /update");
      _T(" /sessionid \"{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_UPDATE;
  expected_.session_id = _T("{756dfdc2-0ef0-44b7-bfb1-21a4be6a1213}");
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /regserver
TEST_F(CommandLineTest, ParseCommandLine_Regserver) {
  const TCHAR* kCmdLine = _T("goopdate.exe /regserver");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_REGSERVER;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /unregserver
TEST_F(CommandLineTest, ParseCommandLine_Unregserver) {
  const TCHAR* kCmdLine = _T("goopdate.exe /unregserver");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_UNREGSERVER;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /registerproduct
TEST_F(CommandLineTest, ParseCommandLine_RegisterProduct) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /registerproduct ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_REGISTER_PRODUCT;
  expected_.extra_args_str = _T("appguid=")
                             _T("{A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                             _T("appname=YouTubeUploader&needsadmin=False");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, false);
}

// Parse: <path> /registerproduct /installsource enterprisemsi
TEST_F(CommandLineTest, ParseCommandLine_RegisterProductWithInstallSource) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /registerproduct ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False\"")
      _T(" /installsource enterprisemsi");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));
  expected_.mode = COMMANDLINE_MODE_REGISTER_PRODUCT;
  expected_.install_source = _T("enterprisemsi");
  expected_.extra_args_str = _T("appguid=")
                             _T("{A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                             _T("appname=YouTubeUploader&needsadmin=False");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, false);
}

// Parse: <path> /unregisterproduct
TEST_F(CommandLineTest, ParseCommandLine_UnregisterProduct) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /unregisterproduct ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_UNREGISTER_PRODUCT;
  expected_.extra_args_str = _T("appguid=")
                             _T("{A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                             _T("appname=YouTubeUploader&needsadmin=False");
  VerifyArgsWithSingleYouTubeUploaderEnApp(expected_, args_, false, false);
}

// Parse: <path> /c
TEST_F(CommandLineTest, ParseCommandLine_Core) {
  const TCHAR* kCmdLine = _T("goopdate.exe /c");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_CORE;
  expected_.is_crash_handler_disabled = false;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /c /nocrashserver
TEST_F(CommandLineTest, ParseCommandLine_CoreNoCrashHandler) {
  const TCHAR* kCmdLine = _T("goopdate.exe /c /nocrashserver");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_CORE;
  expected_.is_crash_handler_disabled = true;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /crash
TEST_F(CommandLineTest, ParseCommandLine_Crash) {
  const TCHAR* kCmdLine = _T("goopdate.exe /crash");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_CRASH;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /report crash_file
TEST_F(CommandLineTest, ParseCommandLine_Report) {
  const TCHAR* kCmdLine = _T("goopdate.exe /report C:\\foo\\crash.dmp");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_REPORTCRASH;
  expected_.crash_filename = _T("C:\\foo\\crash.dmp");
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /report crash_file /machine
TEST_F(CommandLineTest, ParseCommandLine_ReportMachine) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /report C:\\foo\\crash.dmp /machine");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_REPORTCRASH;
  expected_.crash_filename = _T("C:\\foo\\crash.dmp");
  expected_.is_machine_set = true;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /report crash_file
TEST_F(CommandLineTest, ParseCommandLine_ReportWithCustomInfo) {
  const TCHAR* kCmdLine =
    _T("goopdate.exe /report C:\\foo.dmp /custom_info_filename C:\\foo.txt");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_REPORTCRASH;
  expected_.crash_filename = _T("C:\\foo.dmp");
  expected_.custom_info_filename = _T("C:\\foo.txt");
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /report /i crash_file
TEST_F(CommandLineTest, ParseCommandLine_ReportInteractive) {
  const TCHAR* kCmdLine = _T("goopdate.exe /report /i C:\\foo\\crash.dmp");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_REPORTCRASH;
  expected_.is_interactive_set = true;
  expected_.crash_filename = _T("C:\\foo\\crash.dmp");
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /report /i crash_file /machine
TEST_F(CommandLineTest, ParseCommandLine_ReportMachineInteractive) {
  const TCHAR*
      kCmdLine = _T("goopdate.exe /report /i C:\\foo\\crash.dmp /machine");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_REPORTCRASH;
  expected_.is_machine_set = true;
  expected_.is_interactive_set = true;
  expected_.crash_filename = _T("C:\\foo\\crash.dmp");
  VerifyCommandLineArgs(expected_, args_);
}

TEST_F(CommandLineTest, ParseCommandLine_CodeRedCheck) {
  const TCHAR* kCmdLine = _T("goopdate.exe /cr");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_CODE_RED_CHECK;
  VerifyCommandLineArgs(expected_, args_);
}

TEST_F(CommandLineTest, ParseCommandLine_LegacyOmaha1UiNoLanguage) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ui \"manifestfilename.xml\"");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, ParseCommandLine_LegacyOmaha1UiWithLanguage) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /ui /lang fr \"manifestfilename.xml\"");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, ParseCommandLine_LegacyOmaha1UiUser) {
  const TCHAR* kCmdLine = _T("goopdate.exe /uiuser file.gup");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, ParseCommandLine_Recover) {
  const TCHAR* kCmdLine = _T("goopdate.exe /recover repairfile.exe");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_RECOVER;
  expected_.code_red_metainstaller_path = _T("repairfile.exe");
  VerifyCommandLineArgs(expected_, args_);
}

TEST_F(CommandLineTest, ParseCommandLine_RecoverMachine) {
  const TCHAR* kCmdLine = _T("goopdate.exe /recover /machine repfile.exe");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_RECOVER;
  expected_.is_machine_set = true;
  expected_.code_red_metainstaller_path = _T("repfile.exe");
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /uninstall
TEST_F(CommandLineTest, ParseCommandLine_Uninstall) {
  const TCHAR* kCmdLine = _T("goopdate.exe /uninstall");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_UNINSTALL;
  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /ping foo
TEST_F(CommandLineTest, ParseCommandLine_Ping) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ping foo");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_PING;
  expected_.ping_string = _T("foo");
  VerifyCommandLineArgs(expected_, args_);
}

//
// These are additional failure cases against the command line parsing.
// Everything from here on down should fail ParseCommandLine().
//


// Parse: <path> manifest_file
TEST_F(CommandLineTest, ParseCommandLine_GoopdateJustArg) {
  const TCHAR* kCmdLine = _T("goopdate.exe \"foo bar\"");
  ExpectAsserts expect_asserts;
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /install manifest_file manifest_file
// Fails since this is an invalid command line set.
TEST_F(CommandLineTest, ParseCommandLine_Invalid) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install \"foo bar\" foobar");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /recover
TEST_F(CommandLineTest, Recover_WithoutFile) {
  const TCHAR* kCmdLine = _T("goopdate.exe /recover");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /machine
TEST_F(CommandLineTest, MachineWithoutRecover) {
  const TCHAR* kCmdLine = _T("goopdate.exe /machine");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, ExtraArgsHasDoubleQuoteInTheMiddle) {
  const TCHAR* kCmdLine = _T("goopdate.exe /install \"some_\"file\"");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, CommandsNotSeparatedBySpaces) {
  const TCHAR* kCmdLine = _T("goopdate.exe /recover/machine");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, CommandsDoNotHaveForwardSlashes) {
  const TCHAR* kCmdLine = _T("goopdate.exe recover machine");
  ExpectAsserts expect_asserts;
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, UnknownParameter) {
  const TCHAR* kCmdLine = _T("goopdate.exe /someunknowncommand");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, UiWithLangNoLanguage) {
  const TCHAR* kCmdLine = _T("goopdate.exe /ui /lang \"manifestfilename.xml\"");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /handoff "extraargs" /lang "en"
TEST_F(CommandLineTest, ParseCommandLine_HandoffLegacyOmaha1ToOmaha2) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /handoff ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False\"")
      _T(" /lang en");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /handoff "extraargs" /installsource "asd" /lang "en"
TEST_F(CommandLineTest,
       ParseCommandLine_HandoffWithSourceLegacyOmaha1ToOmaha2) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /handoff ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False\"")
      _T(" /installsource taggedmi /lang en");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /handoff "extraargs" /installsource "taggedmi" /lang "en"
TEST_F(CommandLineTest,
       ParseCommandLine_HandoffWithSourceLegacyOmaha1ToOmaha2Both) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /handoff ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")
      _T(" /installsource taggedmi /lang en");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /install "extraargs" /lang en
TEST_F(CommandLineTest, ParseCommandLine_InstallLegacyOmaha1) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False&")
      _T("appguid={C7A9A2F5-C4F9-42d3-8A8B-55086A205468}&")
      _T("appname=TestApp&needsadmin=true\" /lang en");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

// Parse: <path> /install "extraargs" /installsource taggedmi /lang en
TEST_F(CommandLineTest, ParseCommandLine_InstallWithSourceLegacyOmaha1) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ")
      _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=False\"")
      _T(" /installsource taggedmi /lang en");
  EXPECT_FAILED(ParseCommandLine(kCmdLine, &args_));
}

TEST_F(CommandLineTest, ParseCommandLine_NeedsAdmin_Prefers) {
  const TCHAR* kCmdLine =
      _T("goopdate.exe /install ")
      _T("\"bundlename=My%20Bundle&")
      _T("appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
      _T("appname=YouTubeUploader&needsadmin=Prefers&\"");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_INSTALL;

  expected_.extra_args_str = _T("bundlename=My%20Bundle&appguid=")
                             _T("{A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&")
                             _T("appname=YouTubeUploader&needsadmin=Prefers&");
  CommandLineAppArgs app_args;
  const GUID expected_guid = {0xA4F7B07B, 0xB9BD, 0x4A33,
                              {0xB1, 0x36, 0x96, 0xD2, 0xAD, 0xFB, 0x60, 0xCB}};
  app_args.app_guid = expected_guid;
  app_args.app_name = _T("YouTubeUploader");
  app_args.needs_admin = NEEDS_ADMIN_PREFERS;
  expected_.extra.apps.push_back(app_args);

  expected_.extra.bundle_name = _T("My Bundle");

  VerifyCommandLineArgs(expected_, args_);
}

// Parse: <path> /healthcheck
TEST_F(CommandLineTest, ParseCommandLine_HealthCheck) {
  const TCHAR* kCmdLine = _T("goopdate.exe /healthcheck");
  EXPECT_SUCCEEDED(ParseCommandLine(kCmdLine, &args_));

  expected_.mode = COMMANDLINE_MODE_HEALTH_CHECK;
  VerifyCommandLineArgs(expected_, args_);
}

}  // namespace omaha
