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

#include "omaha/base/app_util.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/utils.h"
#include "omaha/common/webplugin_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace webplugin_utils {

#define YOUTUBEUPLOADEREN_TAG \
    _T("\"appguid={A4F7B07B-B9BD-4a33-B136-96D2ADFB60CB}&") \
    _T("appname=YouTubeUploader&needsadmin=False&lang=en\"")


TEST(WebPluginUtilsTest, SanitizeExtraArgs_Valid) {
  CString extra_args = _T("appguid={8A69D345-D564-463c-AFF1-A69D9E530F96}")
                       _T("&appname=Google%20Chrome&needsadmin=true&lang=en");

  CString escaped_extra_args;
  EXPECT_SUCCEEDED(SanitizeExtraArgs(extra_args, &escaped_extra_args));
  EXPECT_STREQ(extra_args, escaped_extra_args);
}

TEST(WebPluginUtilsTest, SanitizeExtraArgs_Invalid) {
  CString extra_args = _T("\"appguid={8A69D345-D564-463c-AFF1-A69D9E530F96}")
                       _T("&appname=Google Chrome&needsadmin=true&lang=en\"")
                       _T(" /offline 1");

  CString escaped_extra_args;
  EXPECT_SUCCEEDED(SanitizeExtraArgs(extra_args, &escaped_extra_args));
  EXPECT_STREQ(_T("%22appguid={8A69D345-D564-463c-AFF1-A69D9E530F96}&")
               _T("appname=Google%20Chrome&needsadmin=true&lang=en%22")
               _T("%20%2Foffline%201"),
               escaped_extra_args);
}

TEST(WebPluginUtilsTest, BuildWebPluginCommandLine_Valid) {
  CString url_domain(_T("http://www.google.com/"));
  CString extra_args = _T("appguid={8A69D345-D564-463c-AFF1-A69D9E530F96}")
                       _T("&appname=Google%20Chrome&needsadmin=true&lang=en");

  CString final_cmd_line_args;
  EXPECT_SUCCEEDED(BuildWebPluginCommandLine(url_domain,
                                             extra_args,
                                             &final_cmd_line_args));
  EXPECT_STREQ(
      _T("\"http:%2F%2Fwww.google.com%2F\" \"%2Finstall%20%22appguid=")
      _T("%7B8A69D345-D564-463c-AFF1-A69D9E530F96%7D%26appname=")
      _T("Google%2520Chrome%26needsadmin=true%26lang=")
      _T("en%22\" /installsource oneclick"),
      final_cmd_line_args);
}

TEST(WebPluginUtilsTest, BuildWebPluginCommandLine_Invalid) {
  CString url_domain(_T("http://www.google.com/"));
  CString extra_args = _T("\"appguid={8A69D345-D564-463c-AFF1-A69D9E530F96}")
                       _T("&appname=Google Chrome&needsadmin=true&lang=en\"")
                       _T(" /offline 1");

  ExpectAsserts expect_asserts;
  CString final_cmd_line_args;
  EXPECT_SUCCEEDED(BuildWebPluginCommandLine(url_domain,
                                             extra_args,
                                             &final_cmd_line_args));
  EXPECT_STREQ(
      _T("\"http:%2F%2Fwww.google.com%2F\" \"%2Finstall%20%22%2522appguid=")
      _T("%7B8A69D345-D564-463c-AFF1-A69D9E530F96%7D%26appname=")
      _T("Google%2520Chrome%26needsadmin=true%26lang=")
      _T("en%2522%2520%252Foffline%25201%22\" /installsource oneclick"),
      final_cmd_line_args);
}

TEST(WebPluginUtilsTest, BuildOneClickWorkerArgs_Valid) {
  CommandLineArgs args;
  CString oneclick_args;

  args.install_source = _T("oneclick");
  args.webplugin_args = _T("/install \"appguid=")
      _T("{8A69D345-D564-463c-AFF1-A69D9E530F96}")
      _T("&appname=Google Chrome&needsadmin=true&lang=en\"");
  EXPECT_EQ(S_OK, BuildOneClickWorkerArgs(args, &oneclick_args));

  EXPECT_STREQ(_T("/install ")
               _T("\"appguid={8A69D345-D564-463c-AFF1-A69D9E530F96}")
               _T("&appname=Google Chrome&needsadmin=true&lang=en\" ")
               _T("/installsource oneclick"),
               oneclick_args);
}

// This tests valid command line args that are not valid to be sent through
// to google_update.exe (e.g. /install).
TEST(WebPluginUtilsTest, BuildOneClickWorkerArgs_Invalid) {
  CommandLineArgs args;
  CString oneclick_args;

  args.install_source = _T("oneclick");

  args.webplugin_args = _T("/handoff ") YOUTUBEUPLOADEREN_TAG;
  EXPECT_EQ(E_INVALIDARG, BuildOneClickWorkerArgs(args, &oneclick_args));

  args.webplugin_args = _T("/regserver");
  EXPECT_EQ(E_INVALIDARG, BuildOneClickWorkerArgs(args, &oneclick_args));

  args.webplugin_args = _T("/unregserver");
  EXPECT_EQ(E_INVALIDARG, BuildOneClickWorkerArgs(args, &oneclick_args));

  args.webplugin_args = _T("/install ") YOUTUBEUPLOADEREN_TAG _T(" /silent");
  EXPECT_EQ(E_INVALIDARG, BuildOneClickWorkerArgs(args, &oneclick_args));
}

TEST(WebPluginUtilsTest, CopyGoopdateToTempDir) {
  CPath current_goopdate_path(app_util::GetCurrentModuleDirectory());
  current_goopdate_path.Append(_T("unittest_support\\omaha_1.3.x\\"));
  CPath goopdate_temp_path;
  ASSERT_SUCCEEDED(CopyGoopdateToTempDir(current_goopdate_path,
                                         &goopdate_temp_path));

  std::vector<CString> files;
  EXPECT_HRESULT_SUCCEEDED(FindFilesEx(goopdate_temp_path, _T("*.*"), &files));

  EXPECT_EQ(3, files.size());

  std::map<CString, int> files_map;
  for (size_t file_index = 0; file_index < files.size(); ++file_index) {
    files_map[files[file_index]] = 1;
  }

  EXPECT_TRUE(files_map.find(_T("GoogleUpdate.exe")) != files_map.end());
  EXPECT_TRUE(files_map.find(_T("goopdate.dll")) != files_map.end());
  EXPECT_TRUE(files_map.find(_T("goopdateres_en.dll")) != files_map.end());

  EXPECT_HRESULT_SUCCEEDED(DeleteDirectory(goopdate_temp_path));
}

TEST(WebPluginUtilsTest, IsLanguageSupported_InvalidArgs) {
  CString args = _T("/en");
  EXPECT_FAILED(IsLanguageSupported(args));
}

TEST(WebPluginUtilsTest, IsLanguageSupported_LangOK) {
  CString args = _T("/install \"appguid=")
                 _T("{8A69D345-D564-463c-AFF1-A69D9E530F96}")
                 _T("&appname=Google Chrome&needsadmin=true&lang=en\"");
  EXPECT_SUCCEEDED(IsLanguageSupported(args));
}

TEST(WebPluginUtilsTest, IsLanguageSupported_LangNotFound) {
  CString args = _T("/install \"appguid=")
                 _T("{8A69D345-D564-463c-AFF1-A69D9E530F96}")
                 _T("&appname=Google Chrome&needsadmin=true&lang=zz\"");

  EXPECT_EQ(GOOPDATE_E_ONECLICK_LANGUAGE_NOT_SUPPORTED,
            IsLanguageSupported(args));
}

}  // namespace webplugin_utils

}  // namespace omaha

