// Copyright 2010 Google Inc.
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

#include <windows.h>
#include <atlpath.h>
#include <atlsecurity.h>
#include <atlstr.h>

#include <regex>
#include <vector>

#include "omaha/base/error.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/client/help_url_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/net/http_client.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace {

#define APP_GUID  _T("{B7BAF788-9D64-49c3-AFDC-B336AB12F332}")
#define APP_GUID2 _T("{6D2DF75B-11F0-41CA-9874-79DE4568527C}")
const TCHAR* const kAppGuid  = APP_GUID;
const TCHAR* const kAppGuid2 = APP_GUID2;

const TCHAR kStringAlmostTooLongForUrl[] =
    _T("000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000");  // NOLINT

// Verifies that one of the expected OS strings was found in the url.
// Returns the position along with the length of the OS string.
int VerifyOSInUrl(const CString& url, int* length) {
  ASSERT1(length);
  *length = 0;

  const std::wregex expected_os_string {
      _T("(?:5\\.[12]|6\\.[013]|10\\.0)\\.\\d+\\.\\d+")
      _T("&sp=(?:Service%20Pack%20[123])?")
  };

  std::wcmatch m;
  EXPECT_TRUE(std::regex_search(url.GetString(), m, expected_os_string));
  EXPECT_EQ(1, m.size());

  *length = m.length(0);
  return url.Find(m.str(0).c_str());
}

}  // namespace

class HelpUrlBuilderTest : public testing::Test {
 protected:
  HRESULT BuildHttpGetString(
      const CString& base_url,
      const std::vector<HelpUrlBuilder::AppResult>& app_results,
      const CString& goopdate_version,
      bool is_machine,
      const CString& language,
      const GUID& iid,
      const CString& brand_code,
      const CString& source_id,
      CString* get_request) const {
    HelpUrlBuilder url_builder(is_machine, language, iid, brand_code);
    return url_builder.BuildHttpGetString(base_url,
                                          app_results,
                                          goopdate_version,
                                          source_id,
                                          get_request);
  }

  HelpUrlBuilderTest() : hive_override_key_name_(kRegistryHiveOverrideRoot),
                         module_version_(GetVersion()) {
  }

  virtual void SetUp() {
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);
    InitializeVersion(kFakeVersion);
  }

  virtual void TearDown() {
    InitializeVersion(module_version_);
    RestoreRegistryHives();
    ASSERT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
  }

  CString hive_override_key_name_;
  const ULONGLONG module_version_;

  static const ULONGLONG kFakeVersion = 0x0005000600070008;
};

TEST_F(HelpUrlBuilderTest, BuildHttpGetString_MachineNoTestSource) {
  CString expected_str_before_os(
      _T("https://www.google.com/hello.py?code=123&hl=en&")
      _T("guver=1.0.51.0&m=1&os="));
  CString expected_str_after_os(
      _T("&iid=%7B0F973A20-C484-462B-952C-5D9A459E3326%7D")  // Upper case 'B'.
      _T("&brand=GoOG&source=click"));
  bool expected_test_source = false;

#if defined(DEBUG) || !OFFICIAL_BUILD
  // TestSource is always set for these builds. It may be set for opt official
  // builds but this is not guaranteed.
  expected_str_after_os.Append(_T("&testsource="));
  expected_test_source = true;
#endif

  CString url_req;
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(kAppGuid, 10, 22));
  EXPECT_SUCCEEDED(BuildHttpGetString(
      _T("https://www.google.com/hello.py?code=123&"),
      app_results,
      _T("1.0.51.0"),
      true,
      _T("en"),
      StringToGuid(_T("{0F973A20-C484-462b-952C-5D9A459E3326}")),
      _T("GoOG"),
      _T("click"),
      &url_req));

  EXPECT_EQ(-1, url_req.FindOneOf(_T("{}")));

  EXPECT_LE(expected_str_before_os.GetLength(), url_req.GetLength());
  EXPECT_EQ(0, url_req.Find(expected_str_before_os)) <<
      _T("Expected: ") << expected_str_before_os.GetString() << std::endl <<
      _T("At beginning of: ") << url_req.GetString();
  int os_fragment_len = 0;
  EXPECT_EQ(expected_str_before_os.GetLength(),
            VerifyOSInUrl(url_req, &os_fragment_len)) <<
      _T("Expected OS string not found in: ") << url_req.GetString();

  EXPECT_EQ(expected_str_before_os.GetLength() + os_fragment_len,
            url_req.Find(expected_str_after_os)) <<
      _T("Expected: ") << expected_str_after_os.GetString() << std::endl <<
      _T("At end of: ") << url_req.GetString();

  if (expected_test_source) {
    CString expected_testsource_str =
        ConfigManager::Instance()->GetTestSource();
    int expected_testsource_start = expected_str_before_os.GetLength() +
                                    os_fragment_len +
                                    expected_str_after_os.GetLength();
    EXPECT_EQ(expected_testsource_start, url_req.Find(expected_testsource_str));
    EXPECT_EQ(expected_testsource_start + expected_testsource_str.GetLength(),
              url_req.GetLength());
  } else {
    EXPECT_EQ(expected_str_before_os.GetLength() +
              os_fragment_len +
              expected_str_after_os.GetLength(),
              url_req.GetLength());

    EXPECT_EQ(-1, url_req.Find(_T("testsource")));
  }
}

TEST_F(HelpUrlBuilderTest, BuildHttpGetString_UserWithTestSource) {
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE_DEV,
                                    kRegValueTestSource,
                                    _T("dev")));

  const CString expected_str_before_os(
      _T("https://www.google.com/hello.py?hl=de&")
      _T("product=%7BB7BAF788-9D64-49c3-AFDC-B336AB12F332%7D&")
      _T("error=0xffffffff&extra_code=99&")
      _T("guver=foo%20bar&m=0&os="));
  const CString expected_str_after_os(
      _T("&iid=%7B0F973A20-C484-462B-952C-5D9A459E3326%7D")  // Upper case 'B'.
      _T("&brand=GGLE&source=clack")
      _T("&testsource="));

  CString url_req;
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(kAppGuid, 0xffffffff, 99));
  EXPECT_SUCCEEDED(BuildHttpGetString(
      _T("https://www.google.com/hello.py?"),
      app_results,
      _T("foo bar"),
      false,
      _T("de"),
      StringToGuid(_T("{0F973A20-C484-462b-952C-5D9A459E3326}")),
      _T("GGLE"),
      _T("clack"),
      &url_req));
  EXPECT_LE(expected_str_before_os.GetLength(), url_req.GetLength());
  EXPECT_EQ(0, url_req.Find(expected_str_before_os));

  int os_fragment_len = 0;
  EXPECT_EQ(expected_str_before_os.GetLength(),
            VerifyOSInUrl(url_req, &os_fragment_len)) <<
      _T("Expected: ") << expected_str_before_os.GetString() << std::endl <<
      _T("At beginning of: ") << url_req.GetString();

  EXPECT_EQ(expected_str_before_os.GetLength() + os_fragment_len,
            url_req.Find(expected_str_after_os)) <<
      _T("Expected OS string not found in: ") << url_req.GetString();

  const CString expected_testsource_str = _T("dev");

  int expected_testsource_start = expected_str_before_os.GetLength() +
                                  os_fragment_len +
                                  expected_str_after_os.GetLength();
  EXPECT_EQ(expected_testsource_start, url_req.Find(expected_testsource_str));
  EXPECT_EQ(expected_testsource_start + expected_testsource_str.GetLength(),
            url_req.GetLength());
}

// IID and brand code are emtpy if not present.
TEST_F(HelpUrlBuilderTest, BuildHttpGetString_NoIidOrBrandCode) {
  const CString expected_str_before_os(
      _T("https://www.google.com/hello.py?hl=en&")
      _T("product=%7BB7BAF788-9D64-49c3-AFDC-B336AB12F332%7D&")
      _T("error=0xffffffff&extra_code=99&")
      _T("guver=foo%20bar&m=1&os="));
  const CString expected_str_after_os(_T("&iid=&brand=&source=cluck"));

  CString url_req;
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(
      _T("{B7BAF788-9D64-49c3-AFDC-B336AB12F332}"), 0xffffffff, 99));
  EXPECT_SUCCEEDED(BuildHttpGetString(
      _T("https://www.google.com/hello.py?"),
      app_results,
      _T("foo bar"),
      true,
      _T("en"),
      GUID_NULL,
      _T(""),
      _T("cluck"),
      &url_req));

  EXPECT_EQ(0, url_req.Find(expected_str_before_os)) <<
      _T("Expected: ") << expected_str_before_os.GetString() << std::endl <<
      _T("At beginning of: ") << url_req.GetString();

  EXPECT_LT(0, url_req.Find(expected_str_after_os));

  CString expected_test_src;
#if defined(DEBUG) || !OFFICIAL_BUILD
  expected_test_src = _T("&testsource=auto");
#endif
  const CString expected_iid_str(_T("&iid=&brand=&source=cluck"));
  EXPECT_EQ(url_req.GetLength() -
            expected_iid_str.GetLength() -
            expected_test_src.GetLength(),
            url_req.Find(expected_iid_str));
}

TEST_F(HelpUrlBuilderTest, BuildHttpGetString_UrlTooLong) {
  EXPECT_LT(INTERNET_MAX_URL_LENGTH, arraysize(kStringAlmostTooLongForUrl) + 5);

  ExpectAsserts expect_asserts;  // BuildHttpGetString asserts on URL length.
  CString url_req;
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(
      _T("{B7BAF788-9D64-49c3-AFDC-B336AB12F332}"), 0xffffffff, 99));
  EXPECT_EQ(E_FAIL, BuildHttpGetString(
      _T("https://www.google.com/hello.py?"),
      app_results,
      _T("foo bar"),
      true,
      _T("en"),
      GUID_NULL,
      _T(""),
      kStringAlmostTooLongForUrl,
      &url_req));
}

TEST_F(HelpUrlBuilderTest, BuildHttpGetString_MultipleApps) {
  CString expected_str_before_os(
      _T("https://www.google.com/hello.py?code=123&hl=en&")
      _T("product=%7BB7BAF788-9D64-49c3-AFDC-B336AB12F332%7D&")
      _T("error=0x80000001&extra_code=1000&")
      _T("guver=1.0.51.22&m=1&os="));
  CString expected_str_after_os(
      _T("&iid=%7B0F973A20-C484-462B-952C-5D9A459E3326%7D")  // Upper case 'B'.
      _T("&brand=TEST&source=click"));
  bool expected_test_source = false;

#if defined(DEBUG) || !OFFICIAL_BUILD
  // TestSource is always set for these builds. It may be set for opt official
  // builds but this is not guaranteed.
  expected_str_after_os.Append(_T("&testsource="));
  expected_test_source = true;
#endif

  CString url_req;
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(_T("SucceededApp1"), 0, 0));
  app_results.push_back(HelpUrlBuilder::AppResult(kAppGuid, 0x80000001, 1000));
  app_results.push_back(HelpUrlBuilder::AppResult(kAppGuid2, 0, 0));
  EXPECT_SUCCEEDED(BuildHttpGetString(
      _T("https://www.google.com/hello.py?code=123&"),
      app_results,
      _T("1.0.51.22"),
      true,
      _T("en"),
      StringToGuid(_T("{0F973A20-C484-462b-952C-5D9A459E3326}")),
      _T("TEST"),
      _T("click"),
      &url_req));

  EXPECT_EQ(-1, url_req.FindOneOf(_T("{}")));

  EXPECT_LE(expected_str_before_os.GetLength(), url_req.GetLength());
  EXPECT_EQ(0, url_req.Find(expected_str_before_os)) <<
      _T("Expected: ") << expected_str_before_os.GetString() << std::endl <<
      _T("At beginning of: ") << url_req.GetString();
  int os_fragment_len = 0;
  EXPECT_EQ(expected_str_before_os.GetLength(),
            VerifyOSInUrl(url_req, &os_fragment_len)) <<
      _T("Expected OS string not found in: ") << url_req.GetString();

  EXPECT_EQ(expected_str_before_os.GetLength() + os_fragment_len,
            url_req.Find(expected_str_after_os)) <<
      _T("Expected: ") << expected_str_after_os.GetString() << std::endl <<
      _T("At end of: ") << url_req.GetString();

  if (expected_test_source) {
    CString expected_testsource_str =
        ConfigManager::Instance()->GetTestSource();
    int expected_testsource_start = expected_str_before_os.GetLength() +
                                    os_fragment_len +
                                    expected_str_after_os.GetLength();
    EXPECT_EQ(expected_testsource_start, url_req.Find(expected_testsource_str));
    EXPECT_EQ(expected_testsource_start + expected_testsource_str.GetLength(),
              url_req.GetLength());
  } else {
    EXPECT_EQ(expected_str_before_os.GetLength() +
              os_fragment_len +
              expected_str_after_os.GetLength(),
              url_req.GetLength());

    EXPECT_EQ(-1, url_req.Find(_T("testsource")));
  }
}

// Machine ID must be set or it will be randomly generated in some cases.
TEST_F(HelpUrlBuilderTest, BuildGetHelpUrl_User) {
  // The URL has a begin, middle which is OS-specific and not checked, and end.
  const CString kExpectedUrlBegin =
      _T("https://www.") COMPANY_DOMAIN _T("/support/installer/?hl=en-GB&")
      _T("product=%7Btest-user-app-id%7D&error=0x80004005&")
      _T("extra_code=-2147418113&guver=5.6.7.8&m=0&os=");
  const CString kExpectedUrlAfterOs = _T("iid=&brand=&source=gethelp")
#if defined(DEBUG) || !OFFICIAL_BUILD
      // TestSource is always set for these builds.
      _T("&testsource=");
#else
      // TestSource never set for other builds because registry is overridden.
      ;  // NOLINT
#endif

  CString url;
  HelpUrlBuilder  url_builder(false, _T("en-GB"), GUID_NULL, _T(""));
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(
      HelpUrlBuilder::AppResult(_T("{test-user-app-id}"),
                                E_FAIL,
                                static_cast<DWORD>(E_UNEXPECTED)));
  EXPECT_SUCCEEDED(url_builder.BuildUrl(app_results, &url));

  EXPECT_STREQ(kExpectedUrlBegin, url.Left(kExpectedUrlBegin.GetLength()));
  EXPECT_NE(-1, url.Find(kExpectedUrlAfterOs))
      << kExpectedUrlAfterOs.GetString() << std::endl
      << _T(" not found in ") << std::endl << url.GetString();
}

TEST_F(HelpUrlBuilderTest, BuildGetHelpUrl_Machine) {
  // The URL has a begin, middle which is OS-specific and not checked, and end.
  const CString kExpectedUrlBegin =
      _T("https://www.") COMPANY_DOMAIN _T("/support/installer/?hl=en-GB&")
      _T("product=%7Btest-machine-app-id%7D&error=0x80004004&")
      _T("extra_code=99&guver=5.6.7.8&m=1&os=");
  const CString kExpectedUrlAfterOs =
      _T("iid=%7B326ADA1D-06AA-4C16-8101-5FC3FEBC852A%7D&")  // Upper case 'C'.
      _T("brand=GOOG&source=gethelp")
#if defined(DEBUG) || !OFFICIAL_BUILD
      // TestSource is always set for these builds.
      _T("&testsource=");
#else
      // TestSource never set for other builds because registry is overridden.
      ;  // NOLINT
#endif

  const GUID kIid = StringToGuid(_T("{326ADA1D-06AA-4c16-8101-5FC3FEBC852A}"));
  CString url;
  HelpUrlBuilder  url_builder(true, _T("en-GB"), kIid, _T("GOOG"));
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(_T("{test-machine-app-id}"),
                                                  E_ABORT,
                                                  99));
  EXPECT_SUCCEEDED(url_builder.BuildUrl(app_results, &url));

  EXPECT_STREQ(kExpectedUrlBegin, url.Left(kExpectedUrlBegin.GetLength()));
  EXPECT_NE(-1, url.Find(kExpectedUrlAfterOs))
      << kExpectedUrlAfterOs.GetString() << std::endl
      << _T(" not found in ") << std::endl << url.GetString();
}

// Use extra code instead if it exists when installer error happens.
TEST_F(HelpUrlBuilderTest, BuildGetHelpUrl_InstallerErrorWithExtraCode) {
  // The URL has a begin, middle which is OS-specific and not checked, and end.
  const CString kExpectedUrlBegin =
      _T("https://www.") COMPANY_DOMAIN _T("/support/installer/?hl=en-GB&")
      _T("product=AppName&error=1666&from_extra_code=1&")
      _T("guver=5.6.7.8&m=1&os=");
  const CString kExpectedUrlAfterOs =
      _T("iid=%7B326ADA1D-06AA-4C16-8101-5FC3FEBC852A%7D&")  // Upper case 'C'.
      _T("brand=GOOG&source=gethelp")
#if defined(DEBUG) || !OFFICIAL_BUILD
      // TestSource is always set for these builds.
      _T("&testsource=");
#else
      // TestSource never set for other builds because registry is overridden.
      ;  // NOLINT
#endif

  const GUID kIid = StringToGuid(_T("{326ADA1D-06AA-4c16-8101-5FC3FEBC852A}"));
  CString url;
  HelpUrlBuilder url_builder(true, _T("en-GB"), kIid, _T("GOOG"));
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(
      _T("AppName"), GOOPDATEINSTALL_E_INSTALLER_FAILED, 1666));
  EXPECT_SUCCEEDED(url_builder.BuildUrl(app_results, &url));

  EXPECT_STREQ(kExpectedUrlBegin, url.Left(kExpectedUrlBegin.GetLength()));
  EXPECT_NE(-1, url.Find(kExpectedUrlAfterOs))
      << kExpectedUrlAfterOs.GetString() << std::endl
      << _T(" not found in ") << std::endl << url.GetString();
}

// Keep using error code if extra code does not have meaningful value when
// installer error happens.
TEST_F(HelpUrlBuilderTest, BuildGetHelpUrl_InstallerErrorWithoutExtraCode) {
  // The URL has a begin, middle which is OS-specific and not checked, and end.
  const CString kExpectedUrlBegin =
      _T("https://www.") COMPANY_DOMAIN _T("/support/installer/?hl=en-GB&")
      _T("product=AppName&error=0x80040902&extra_code=0&")
      _T("guver=5.6.7.8&m=1&os=");
  const CString kExpectedUrlAfterOs =
      _T("iid=%7B326ADA1D-06AA-4C16-8101-5FC3FEBC852A%7D&")  // Upper case 'C'.
      _T("brand=GOOG&source=gethelp")
#if defined(DEBUG) || !OFFICIAL_BUILD
      // TestSource is always set for these builds.
      _T("&testsource=");
#else
      // TestSource never set for other builds because registry is overridden.
      ;  // NOLINT
#endif

  const GUID kIid = StringToGuid(_T("{326ADA1D-06AA-4c16-8101-5FC3FEBC852A}"));
  CString url;
  HelpUrlBuilder  url_builder(true, _T("en-GB"), kIid, _T("GOOG"));
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(HelpUrlBuilder::AppResult(
      _T("AppName"), GOOPDATEINSTALL_E_INSTALLER_FAILED, 0));
  EXPECT_SUCCEEDED(url_builder.BuildUrl(app_results, &url));

  EXPECT_STREQ(kExpectedUrlBegin, url.Left(kExpectedUrlBegin.GetLength()));
  EXPECT_NE(-1, url.Find(kExpectedUrlAfterOs))
      << kExpectedUrlAfterOs.GetString() << std::endl
      << _T(" not found in ") << std::endl << url.GetString();
}

// Makes BuildHttpGetString fail by making the URL too long.
// The call succeeds, but the url is empty.
TEST_F(HelpUrlBuilderTest, BuildGetHelpUrl_BuildFails) {
  EXPECT_LT(INTERNET_MAX_URL_LENGTH, arraysize(kStringAlmostTooLongForUrl) + 5);

  ExpectAsserts expect_asserts;  // BuildHttpGetString asserts on URL length.
  CString url;
  HelpUrlBuilder  url_builder(false, _T("en-GB"), GUID_NULL, _T(""));
  std::vector<HelpUrlBuilder::AppResult> app_results;
  app_results.push_back(
      HelpUrlBuilder::AppResult(kStringAlmostTooLongForUrl,
                                E_FAIL,
                                static_cast<DWORD>(E_UNEXPECTED)));
  EXPECT_EQ(E_FAIL, url_builder.BuildUrl(app_results, &url));
  EXPECT_TRUE(url.IsEmpty());
}

}  // namespace omaha
