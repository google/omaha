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

#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/extra_args_parser.h"
#include "omaha/testing/resource.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

extern void VerifyCommandLineArgs(const CommandLineArgs& expected,
                                  const CommandLineArgs& actual);

extern void VerifyCommandLineExtraArgs(const CommandLineExtraArgs& expected,
                                       const CommandLineExtraArgs& actual);

void VerifyBrowserType(const CommandLineExtraArgs& args,
                       const CString& app_guid,
                       BrowserType type) {
  CommandLineAppArgs app_args;
  app_args.app_guid = StringToGuid(app_guid);

  CommandLineExtraArgs expected;
  expected.browser_type = type;
  expected.apps.push_back(app_args);

  VerifyCommandLineExtraArgs(expected, args);
}

// TODO(omaha): Consider eliminating most uses of this method. The tests will
// actually be less lines now that the parameters have grown so much.
void VerifyExtraArgsHaveSpecificValues(
         const CommandLineExtraArgs& args,
         const CString& expected_app_guid,
         const CString& expected_app_name,
         const CString& expected_app_experiment_labels,
         const CString& expected_untrusted_data,
         NeedsAdmin expected_needs_admin,
         Tristate expected_usage_stats_enable,
         RuntimeMode expected_runtime_mode,
         const CString& expected_bundle_name,
         const GUID& expected_installation_id,
         const CString& expected_brand_code,
         const CString& expected_client_id,
         const CString& expected_omaha_experiment_labels,
         const CString& expected_referral_id,
         const CString& expected_ap,
         const CString& expected_tt,
         const CString& expected_encoded_installer_data,
         const CString& expected_install_data_index) {
  CommandLineAppArgs app_args;
  app_args.app_guid = StringToGuid(expected_app_guid);
  app_args.app_name = expected_app_name;
  app_args.needs_admin = expected_needs_admin;
  app_args.ap = expected_ap;
  app_args.tt_token = expected_tt;
  app_args.encoded_installer_data = expected_encoded_installer_data;
  app_args.install_data_index = expected_install_data_index;
  app_args.experiment_labels = expected_app_experiment_labels;
  app_args.untrusted_data = expected_untrusted_data;

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);
  expected.bundle_name = expected_bundle_name;
  expected.installation_id = expected_installation_id;
  expected.brand_code = expected_brand_code;
  expected.client_id = expected_client_id;
  expected.experiment_labels = expected_omaha_experiment_labels;
  expected.referral_id = expected_referral_id;
  expected.usage_stats_enable = expected_usage_stats_enable;
  expected.runtime_mode = expected_runtime_mode;

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsInvalidValueNameIsSupersetOfValidName) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                       _T("appname1=Hello");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppNameSpaceForValue) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                       _T("appname= ");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppNameValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;

  CString app_guid = _T("{D0324988-DA8A-49e5-BCE5-925FCD04EAB7}");
  CString extra_args = _T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                       _T("appname=Test");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;

  app_args.app_name = _T("Test");
  app_args.app_guid = StringToGuid(app_guid);
  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);
  expected.bundle_name = _T("Test");

  VerifyCommandLineExtraArgs(expected, args);
}

// This must work because the enterprise MSI code assumes spaces are allowed.
TEST(ExtraArgsParserTest, ExtraArgumentsAppNameWithSpace) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                       _T("appname=Test App");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.app_guid =
      StringToGuid(_T("{D0324988-DA8A-49e5-BCE5-925FCD04EAB7}"));
  app_args.app_name = _T("Test App");

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);
  expected.bundle_name = _T("Test App");

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppNameWithSpaceAtEnd) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                       _T("appname= T Ap p ");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.app_guid =
      StringToGuid(_T("{D0324988-DA8A-49e5-BCE5-925FCD04EAB7}"));
  app_args.app_name = _T("T Ap p");

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);
  expected.bundle_name = _T("T Ap p");

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppNameWithMultipleSpaces) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                       _T("appname= T Ap p");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.app_guid =
      StringToGuid(_T("{D0324988-DA8A-49e5-BCE5-925FCD04EAB7}"));
  app_args.app_name = _T("T Ap p");

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);
  expected.bundle_name = _T("T Ap p");

  VerifyCommandLineExtraArgs(expected, args);
}

// The size of the application name is limited to 512 wide chars.
TEST(ExtraArgsParserTest, ExtraArgumentsAppNameTooLong) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString str(_T('a'), 513);
  CString extra_args;
  extra_args.Format(_T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                    _T("appname=%s"), str);
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppNameUnicode) {
  // Read the non-ascii string from the resources, and convert
  // it into a utf8 encoded, url escaped string.
  CString non_ascii_name;
  ASSERT_TRUE(non_ascii_name.LoadString(IDS_ESCAPE_TEST));

  CString wide_tag;
  ASSERT_HRESULT_SUCCEEDED(WideStringToUtf8UrlEncodedString(non_ascii_name,
                                                            &wide_tag));

  ExtraArgsParser parser;
  CommandLineExtraArgs args;
  CString extra_args;
  extra_args.Format(_T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                    _T("appname=%s"), wide_tag);
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.app_name = non_ascii_name;
  app_args.app_guid =
      StringToGuid(_T("{D0324988-DA8A-49e5-BCE5-925FCD04EAB7}"));

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);
  expected.bundle_name = non_ascii_name;

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppNameUnicode2) {
  // Read the non-ascii string from the resources, and convert
  // it into a utf8 encoded, url escaped string.
  CString non_ascii_name;
  ASSERT_TRUE(non_ascii_name.LoadString(IDS_ESCAPE_TEST1));

  CString escaped(_T("%E0%A4%B8%E0%A5%8D%E0%A4%A5%E0%A4%BE%E0%A4%AA%E0%A4%BF")
                  _T("%E0%A4%A4%20%E0%A4%95%E0%A4%B0%20%E0%A4%B0%E0%A4%B9%E0")
                  _T("%A4%BE%20%E0%A4%B9%E0%A5%88%E0%A5%A4"));

  CommandLineExtraArgs args;
  ExtraArgsParser parser;

  CString extra_args;
  extra_args.Format(_T("appguid={D0324988-DA8A-49e5-BCE5-925FCD04EAB7}&")
                    _T("appname=%s"), escaped);
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.app_name = non_ascii_name;
  app_args.app_guid =
      StringToGuid(_T("{D0324988-DA8A-49e5-BCE5-925FCD04EAB7}"));

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);
  expected.bundle_name = non_ascii_name;

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppGuidValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  const GUID guid = {0x8617EE50, 0xF91C, 0x4DC1,
                          {0xB9, 0x37, 0x09, 0x69, 0xEE, 0xF5, 0x9B, 0x0B}};
  app_args.app_guid = guid;

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppGuidNotAValidGuid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0}");
  EXPECT_EQ(E_INVALIDARG, parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsAppGuidNotAGuid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid=myguid");
  EXPECT_EQ(E_INVALIDARG, parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsNeedsAdminInvalid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("needsadmin=Hello");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsNeedsAdminSpaceForValue) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("needsadmin= ");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsNeedsAdminTrueUpperCaseT) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("needsadmin=True");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.needs_admin = NEEDS_ADMIN_YES;
  app_args.app_guid =
      StringToGuid(_T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"));

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsNeedsAdminTrueLowerCaseT) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("needsadmin=true");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.needs_admin = NEEDS_ADMIN_YES;
  app_args.app_guid =
      StringToGuid(_T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"));

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsNeedsFalseUpperCaseF) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("needsadmin=False");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.needs_admin = NEEDS_ADMIN_NO;
  app_args.app_guid =
      StringToGuid(_T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"));

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);

  VerifyCommandLineExtraArgs(expected, args);
}

TEST(ExtraArgsParserTest, ExtraArgumentsNeedsAdminFalseLowerCaseF) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("needsadmin=false");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));

  CommandLineAppArgs app_args;
  app_args.needs_admin = NEEDS_ADMIN_NO;
  app_args.app_guid =
      StringToGuid(_T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"));

  CommandLineExtraArgs expected;
  expected.apps.push_back(app_args);

  VerifyCommandLineExtraArgs(expected, args);
}

//
// Test the handling of the contents of the extra arguments.
//

TEST(ExtraArgsParserTest, ExtraArgumentsAssignmentOnly) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("=");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsExtraAssignment1) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1=");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsExtraAssignment2) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("=usagestats=1");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsExtraAssignment3) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1&=");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsExtraAssignment4) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("=&usagestats=1");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsValueWithoutName) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("=hello");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}


// Also tests ending extra arguments with '='.
TEST(ExtraArgsParserTest, ExtraArgumentsNameWithoutValue) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsNameWithoutValueBeforeNextArgument) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=&client=hello");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest,
     ExtraArgumentsNameWithoutArgumentSeparatorAfterIntValue) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1client=hello");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest,
     ExtraArgumentsNameWithoutArgumentSeparatorAfterStringValue) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=yesclient=hello");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsHaveDoubleAmpersand) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1&&client=hello");

  CString app_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                     _T("installerdata=foobar");

  // Ideally, this would flag an error.
  EXPECT_SUCCEEDED(parser.Parse(extra_args, app_args, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_TRUE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T("hello"),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T("foobar"),
      _T(""));
}

TEST(ExtraArgsParserTest, ExtraArgumentsAmpersandOnly) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("&");

  // Ideally, this would flag an error.
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{00000000-0000-0000-0000-000000000000}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, ExtraArgumentsBeginInAmpersand) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("&appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1");

  // Ideally, this would flag an error.
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_TRUE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, ExtraArgumentsEndInAmpersand) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("&appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1&");

  // Ideally, this would flag an error.
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_TRUE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, ExtraArgumentsEmptyString) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceOnly1) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T(" ");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceOnly2) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\t");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceOnly3) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\r");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceOnly4) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\n");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceOnly5) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\r\n");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}


//
// Test the parsing of the extra command and its arguments into a string.
//

TEST(ExtraArgsParserTest, ExtraArgumentsOneValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("&appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_TRUE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, ExtraArgumentsTwoValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("&appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1&client=hello");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_TRUE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T("hello"),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, ExtraArgumentsHaveSwitchInTheMiddle) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1/other_value=9");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsHaveDoubleQuoteInTheMiddle) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1\"/other_value=9");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest,
       ExtraArgumentsHaveDoubleQuoteInTheMiddleAndNoForwardSlash) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1\"other_value=9");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsHaveSpaceAndForwardSlashBeforeQuote) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1 /other_value=9");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsHaveForwardSlashBeforeQuote) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1/other_value=9");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsSpecifiedTwice) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1\" \"client=10");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceBeforeArgs1) {
  CommandLineExtraArgs args;
  // TODO(omaha): This one passes now due to different whitespace
  // handling.  Remove it?  Is this really a problem to have?
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T(" usagestats=1");
  ExtraArgsParser parser;
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceBeforeArgs2) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\tappguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceBeforeArgs3) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\rappguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceBeforeArgs4) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\nappguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}")
                       _T("&usagestats=1");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsWhiteSpaceBeforeArgs5) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\r\nusagestats=1");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsForwardSlash1) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("/");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsForwardSlash2) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("/ appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsBackwardSlash1) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\\");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsBackwardSlash2) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\\appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ExtraArgumentsBackwardSlash3) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("\\ appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

//
// Test specific extra commands.
//

TEST(ExtraArgsParserTest, UsageStatsOutsideExtraCommand) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("/usagestats");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, UsageStatsOn) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=1");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_TRUE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, UsageStatsOff) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=0");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_FALSE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

// This commandline has no effect, but it's permitted.
TEST(ExtraArgsParserTest, UsageStatsNone) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=2");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, UsageStatsInvalidPositiveValue) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=3");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, UsageStatsInvalidNegativeValue) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=-1");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, UsageStatsValueIsString) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("usagestats=true");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, BundleNameValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("bundlename=Google%20Bundle");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T("Google Bundle"),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, BundleNameNotPresentButAppNameIs) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("appname=Google%20Chrome");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T("Google Chrome"),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T("Google Chrome"),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}
TEST(ExtraArgsParserTest, BundleNameNorAppNamePresent) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, BundleNameNotPresentAndNoApp) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("browser=0");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{00000000-0000-0000-0000-000000000000}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, InstallationGuidValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("iid={98CEC468-9429-4984-AEDE-4F53C6A14869}");
  const GUID expected_guid = {0x98CEC468, 0x9429, 0x4984,
                              {0xAE, 0xDE, 0x4F, 0x53, 0xC6, 0xA1, 0x48, 0x69}};

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      expected_guid,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, InstallationGuidMissingBraces) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("iid=98CEC468-9429-4984-AEDE-4F53C6A14869");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, InstallationGuidMissingDashes) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("iid=98CEC46894294984AEDE4F53C6A14869");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, InstallationGuidMissingCharacter) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("iid=98CEC468-9429-4984-AEDE-4F53C6A1486");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, InstallationGuidIsString) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("iid=hello");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, BrandCodeValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("brand=GOOG");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T("GOOG"),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, BrandCodeTooLong) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("brand=CHMI\xe3\x83\xbb");

  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, ClientIdValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("client=some_partner");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T("some_partner"),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, OmahaExperimentIdValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("omahaexperiments=experiment%3DgroupA%7Cexpir");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T("experiment=groupA|expir"),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, AppExperimentIdValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("experiments=experiment%3DgroupA%7Cexpir");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T("experiment=groupA|expir"),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, ReferralIdValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("referral=ABCD123");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T("ABCD123"),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, ApValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("ap=developer");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T("developer"),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, TTValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("tttoken=7839g93");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T("7839g93"),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, AppArgsValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("tttoken=7839g93");

  CString app_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                     _T("installerdata=%E0%A4foobar");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, app_args, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T("7839g93"),
      _T("%E0%A4foobar"),
      _T(""));
}

TEST(ExtraArgsParserTest, AppArgsInvalidAppGuid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("tttoken=7839g93");

  CString app_args = _T("appguid={E135384F-85A2-4328-B07D-2CF70313D505}&")
                     _T("installerdata=%E0%A4foobar");

  EXPECT_EQ(E_INVALIDARG, parser.Parse(extra_args, app_args, &args));
}

TEST(ExtraArgsParserTest, AppArgsInvalidAttribute) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("tttoken=7839g93");

  CString app_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                     _T("tttoken=foobar");

  EXPECT_EQ(E_INVALIDARG, parser.Parse(extra_args, app_args, &args));
}

TEST(ExtraArgsParserTest, InstallerDataNotAllowedInExtraArgs) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("appname=TestApp2&")
                       _T("needsadmin=true&")
                       _T("installerdata=Hello%20World");

  EXPECT_EQ(E_INVALIDARG, parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, InstallDataIndexValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("installdataindex=foobar");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_NOT_SET,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T("foobar"));
}

TEST(ExtraArgsParserTest, BrowserTypeValid_0) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("browser=0");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyBrowserType(args,
                    _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
                    BROWSER_UNKNOWN);
}

TEST(ExtraArgsParserTest, BrowserTypeValid_1) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("browser=1");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyBrowserType(args,
                    _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
                    BROWSER_DEFAULT);
}

TEST(ExtraArgsParserTest, BrowserTypeValid_2) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("browser=2");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyBrowserType(args,
                    _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
                    BROWSER_IE);
}

TEST(ExtraArgsParserTest, BrowserTypeValid_3) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("browser=3");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyBrowserType(args,
                    _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
                    BROWSER_FIREFOX);
}

TEST(ExtraArgsParserTest, BrowserTypeValid_4) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("browser=4");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyBrowserType(args,
                    _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
                    BROWSER_CHROME);
}

TEST(ExtraArgsParserTest, BrowserTypeInvalid) {
  EXPECT_EQ(5, BROWSER_MAX) <<
      _T("Browser type may have been added. Add new Valid_n test and change ")
      _T("browser values in extra args strings below.");

  CommandLineExtraArgs args1;
  ExtraArgsParser parser1;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("browser=5");

  EXPECT_SUCCEEDED(parser1.Parse(extra_args, NULL, &args1));
  VerifyBrowserType(args1,
                    _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
                    BROWSER_UNKNOWN);

  CommandLineExtraArgs args2;
  ExtraArgsParser parser2;
  extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
               _T("browser=9");

  EXPECT_SUCCEEDED(parser2.Parse(extra_args, NULL, &args2));
  VerifyBrowserType(args2,
                    _T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"),
                    BROWSER_UNKNOWN);
}

TEST(ExtraArgsParserTest, ValidLang) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("lang=en");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  EXPECT_STREQ(_T("en"), args.language);
}

// Language must be passed even if not supported. See http://b/1336966.
TEST(ExtraArgsParserTest, UnsupportedLang) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("lang=foobar");
  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  EXPECT_STREQ(_T("foobar"), args.language);
}

TEST(ExtraArgsParserTest, LangTooLong) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("lang=morethan10chars");
  EXPECT_FAILED(parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, RuntimeValid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("runtime=true");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{00000000-0000-0000-0000-000000000000}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_TRUE,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, RuntimeWithExtraArgs) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("runtime=true&")
                       _T("bundlename=Google%20Bundle&")
                       _T("brand=GOOG&")
                       _T("usagestats=1");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{00000000-0000-0000-0000-000000000000}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_TRUE,
      RUNTIME_MODE_TRUE,
      _T("Google Bundle"),
      GUID_NULL,
      _T("GOOG"),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, RuntimePersist) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("runtime=persist");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{00000000-0000-0000-0000-000000000000}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_PERSIST,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, RuntimeFalse) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("runtime=false");

  EXPECT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
  VerifyExtraArgsHaveSpecificValues(
      args,
      _T("{00000000-0000-0000-0000-000000000000}"),
      _T(""),
      _T(""),
      _T(""),
      NEEDS_ADMIN_NO,
      TRISTATE_NONE,
      RUNTIME_MODE_FALSE,
      _T(""),
      GUID_NULL,
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""),
      _T(""));
}

TEST(ExtraArgsParserTest, RuntimeBeforeAppGuid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("runtime=true&")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}");

  EXPECT_EQ(E_INVALIDARG, parser.Parse(extra_args, NULL, &args));
}

TEST(ExtraArgsParserTest, RuntimeAfterAppGuid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("runtime=true");

  EXPECT_EQ(E_INVALIDARG, parser.Parse(extra_args, NULL, &args));
}

//
// Test multiple applications in the extra arguments
//
TEST(ExtraArgsParserTestMultipleEntries, TestNotStartingWithAppGuid) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appname=TestApp&")
                       _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("appname=TestApp&")
                       _T("appname=false&")
                       _T("iid={98CEC468-9429-4984-AEDE-4F53C6A14869}&")
                       _T("ap=test_ap&")
                       _T("tttoken=foobar&")
                       _T("usagestats=1&")
                       _T("browser=2&");
  EXPECT_HRESULT_SUCCEEDED(parser.Parse(extra_args, NULL, &args));
}

// This also tests that the last occurrence of a global extra arg is the one
// that is saved.
TEST(ExtraArgsParserTestMultipleEntries, ThreeApplications) {
  CommandLineExtraArgs args;
  ExtraArgsParser parser;
  CString extra_args = _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                       _T("appname=TestApp&")
                       _T("needsadmin=false&")
                       _T("iid={98CEC468-9429-4984-AEDE-4F53C6A14869}&")
                       _T("ap=test_ap&")
                       _T("tttoken=foobar&")
                       _T("usagestats=1&")
                       _T("browser=2&")
                       _T("brand=GOOG&")
                       _T("client=_some_client&")
                       _T("experiments=_experiment_a&")
                       _T("untrusteddata=ABCDEFG&")
                       _T("referral=A123456789&")
                       _T("appguid={5E46DE36-737D-4271-91C1-C062F9FE21D9}&")
                       _T("appname=TestApp2&")
                       _T("needsadmin=true&")
                       _T("experiments=_experiment_b&")
                       _T("untrusteddata=1234567&")
                       _T("iid={98CEC468-9429-4984-AEDE-4F53C6A14869}&")
                       _T("ap=test_ap2&")
                       _T("tttoken=foobar2&")
                       _T("usagestats=0&")
                       _T("browser=3&")
                       _T("brand=g00g&")
                       _T("client=_different_client&")
                       _T("appguid={5F46DE36-737D-4271-91C1-C062F9FE21D9}&")
                       _T("appname=TestApp3&")
                       _T("needsadmin=prefers&");

  CString app_args = _T("appguid={5F46DE36-737D-4271-91C1-C062F9FE21D9}&")
                     _T("installerdata=installerdata_app3&")
                     _T("appguid={8617EE50-F91C-4DC1-B937-0969EEF59B0B}&")
                     _T("installerdata=installerdata_app1");

  EXPECT_HRESULT_SUCCEEDED(parser.Parse(extra_args, app_args, &args));

  CommandLineAppArgs input1;
  input1.app_guid = StringToGuid(_T("{8617EE50-F91C-4DC1-B937-0969EEF59B0B}"));
  input1.app_name = _T("TestApp");
  input1.needs_admin = NEEDS_ADMIN_NO;
  input1.ap = _T("test_ap");
  input1.tt_token = _T("foobar");
  input1.encoded_installer_data = _T("installerdata_app1");
  input1.experiment_labels = _T("_experiment_a");
  input1.untrusted_data = _T("A=3&Message=Hello,%20World!");

  CommandLineAppArgs input2;
  input2.app_guid = StringToGuid(_T("{5E46DE36-737D-4271-91C1-C062F9FE21D9}"));
  input2.app_name = _T("TestApp2");
  input2.needs_admin = NEEDS_ADMIN_YES;
  input2.ap = _T("test_ap2");
  input2.tt_token = _T("foobar2");
  input2.experiment_labels = _T("_experiment_b");
  input2.untrusted_data = _T("X=5");

  CommandLineAppArgs input3;
  input3.app_guid = StringToGuid(_T("{5F46DE36-737D-4271-91C1-C062F9FE21D9}"));
  input3.app_name = _T("TestApp3");
  input3.encoded_installer_data = _T("installerdata_app3");
  input3.needs_admin = NEEDS_ADMIN_PREFERS;

  CommandLineExtraArgs expected;
  expected.apps.push_back(input1);
  expected.apps.push_back(input2);
  expected.apps.push_back(input3);
  expected.bundle_name = _T("TestApp");
  expected.installation_id = StringToGuid(
      _T("{98CEC468-9429-4984-AEDE-4F53C6A14869}"));
  expected.brand_code = _T("g00g");
  expected.client_id = _T("_different_client");
  expected.referral_id = _T("A123456789");
  expected.browser_type = BROWSER_FIREFOX;
  expected.usage_stats_enable = TRISTATE_FALSE;

  VerifyCommandLineExtraArgs(expected, args);
}

}  // namespace omaha
