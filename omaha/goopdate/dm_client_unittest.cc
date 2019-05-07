// Copyright 2019 Google LLC.
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

#include "omaha/goopdate/dm_client.h"

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "gtest/gtest-matchers.h"
#include "omaha/base/string.h"
#include "omaha/common/config_manager.h"
#include "omaha/goopdate/dm_storage.h"
#include "omaha/goopdate/dm_storage_test_utils.h"
#include "omaha/net/http_request.h"
#include "omaha/testing/unit_test.h"
#include "wireless/android/enterprise/devicemanagement/proto/dm_api.pb.h"

using ::testing::_;
using ::testing::AllArgs;
using ::testing::HasSubstr;
using ::testing::Return;

// An adapter for Google Mock's HasSubstr matcher that operates on a CString
// argument.
MATCHER_P(CStringHasSubstr, substr, "") {
  return ::testing::Value(std::wstring(arg), HasSubstr(substr));
}

namespace omaha {
namespace dm_client {

namespace {

// A Google Mock matcher that returns true if a string contains a valid
// URL to the device management server with the required query parameters.
class IsValidRequestUrlMatcher
    : public ::testing::MatcherInterface<const CString&> {
 public:
  IsValidRequestUrlMatcher(const TCHAR* request_type, const TCHAR* device_id)
      : request_type_(request_type), device_id_(device_id) {
    ConfigManager::Instance()->GetDeviceManagementUrl(&device_management_url_);
  }

  virtual bool MatchAndExplain(const CString& arg,
                               ::testing::MatchResultListener* listener) const {
    // Verify that the base of the URL is the device management server's
    // endpoint.
    int scan = 0;
    CString url = arg.Tokenize(_T("?"), scan);
    if (url != device_management_url_) {
      *listener << "the base url is " << WideToUtf8(url);
      return false;
    }

    // Extract the query params from the URL.
    std::map<CString,CString> query_params;
    CString param = arg.Tokenize(_T("&"), scan);
    while (!param.IsEmpty()) {
      int eq = param.Find('=', 0);
      if (eq == -1) {
        query_params[param] = CString();
      } else {
        query_params[param.Left(eq)] = param.Right(param.GetLength() - eq - 1);
      }
      param = arg.Tokenize(_T("&"), scan);
    }

    // Check that the required params are present.
    static const TCHAR* kRequiredParams[] = {
      _T("agent"),
      _T("apptype"),
      _T("deviceid"),
      _T("platform"),
      _T("request"),
    };
    for (size_t i = 0; i < arraysize(kRequiredParams); ++i) {
      const TCHAR* p = kRequiredParams[i];
      if (query_params.find(p) == query_params.end()) {
        *listener << "the url is missing the \"" << WideToUtf8(p)
                  << "\" query parameter";
        return false;
      }
    }

    // Check the value of the request param.
    if (query_params[_T("request")] != request_type_) {
      *listener << "the request query parameter is \""
                << query_params[_T("request")] << "\"";
      return false;
    }

    // Check the value of the device_id param.
    if (query_params[_T("deviceid")] != device_id_) {
      *listener << "the device_id query parameter is \""
                << query_params[_T("device_id")] << "\"";
      return false;
    }

    return true;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "string contains a valid device management request URL";
  }

 private:
  const TCHAR* const request_type_;
  const TCHAR* const device_id_;
  CString device_management_url_;
};

// Returns an IsValidRequestUrl matcher, which takes a CString and matches if
// it is an URL leading to the device management server endpoint, contains all
// required query parameters, and has the given |request_type| and |device_id|.
::testing::Matcher<const CString&> IsValidRequestUrl(const TCHAR* request_type,
                                                     const TCHAR* device_id) {
  return ::testing::MakeMatcher(
      new IsValidRequestUrlMatcher(request_type, device_id));
}

// A Google Mock matcher that returns true if a buffer contains a valid
// serialized RegisterBrowserRequest message. While the presence of each field
// in the request is checked, the exact value of each is not.
class IsRegisterBrowserRequestMatcher
    : public ::testing::MatcherInterface<const ::testing::tuple<const void*,
                                                                size_t>& > {
 public:
  virtual bool MatchAndExplain(
      const ::testing::tuple<const void*, size_t>& buffer,
      ::testing::MatchResultListener* listener) const {
    enterprise_management::DeviceManagementRequest request;
    if (!request.ParseFromArray(
            ::testing::get<0>(buffer),
            static_cast<int>(::testing::get<1>(buffer)))) {
      *listener << "parse failure";
      return false;
    }
    if (!request.has_register_browser_request()) {
      *listener << "missing register_browser_request";
      return false;
    }
    const enterprise_management::RegisterBrowserRequest& register_request =
        request.register_browser_request();
    if (!register_request.has_machine_name()) {
      *listener << "missing register_browser_request.machine_name";
      return false;
    }
    if (!register_request.has_os_platform()) {
      *listener << "missing register_browser_request.os_platform";
      return false;
    }
    if (!register_request.has_os_version()) {
      *listener << "missing register_browser_request.os_version";
      return false;
    }
    return true;
  }

  virtual void DescribeTo(::std::ostream* os) const {
    *os << "buffer contains a valid serialized RegisterBrowserRequest";
  }
};

// Returns an IsRegisterBrowserRequest matcher, which takes a tuple of a pointer
// to a buffer and a buffer size.
::testing::Matcher<const ::testing::tuple<const void*, size_t>& >
IsRegisterBrowserRequest() {
  return ::testing::MakeMatcher(new IsRegisterBrowserRequestMatcher);
}

class MockHttpRequest : public HttpRequestInterface {
 public:
  MOCK_METHOD0(Close, HRESULT());
  MOCK_METHOD0(Send, HRESULT());
  MOCK_METHOD0(Cancel, HRESULT());
  MOCK_METHOD0(Pause, HRESULT());
  MOCK_METHOD0(Resume, HRESULT());
  MOCK_CONST_METHOD0(GetResponse, std::vector<uint8>());
  MOCK_CONST_METHOD0(GetHttpStatusCode, int());
  MOCK_CONST_METHOD3(QueryHeadersString,
                     HRESULT(uint32 info_level,
                             const TCHAR* name,
                             CString* value));
  MOCK_CONST_METHOD0(GetResponseHeaders, CString());
  MOCK_CONST_METHOD0(ToString, CString());
  MOCK_METHOD1(set_session_handle, void(HINTERNET session_handle));
  MOCK_METHOD1(set_url, void(const CString& url));
  MOCK_METHOD2(set_request_buffer, void(const void* buffer,
                                        size_t buffer_length));
  MOCK_METHOD1(set_proxy_configuration, void(const ProxyConfig& proxy_config));
  MOCK_METHOD1(set_filename, void(const CString& filename));
  MOCK_METHOD1(set_low_priority, void(bool low_priority));
  MOCK_METHOD1(set_callback, void(NetworkRequestCallback* callback));
  MOCK_METHOD1(set_additional_headers, void(const CString& additional_headers));
  MOCK_CONST_METHOD0(user_agent, CString());
  MOCK_METHOD1(set_user_agent, void(const CString& user_agent));
  MOCK_METHOD1(set_proxy_auth_config, void(const ProxyAuthConfig& config));
  MOCK_CONST_METHOD1(download_metrics, bool(DownloadMetrics* download_metrics));
};

}  // namespace

// A test harness for testing DmClient request/response handling.
class DmClientRequestTest : public ::testing::Test {
 protected:
  DmClientRequestTest() {}
  virtual ~DmClientRequestTest() {}

  // Populates |request| with a mock HttpRequest that behaves as if the server
  // successfully processed a RegisterBrowserRequest, returning a
  // DeviceRegisterResponse containing |dm_token|.
  // Note: always wrap calls to this with ASSERT_NO_FATAL_FAILURE.
  void MakeSuccessHttpRequest(const char* dm_token, MockHttpRequest** request) {
    *request = new ::testing::NiceMock<MockHttpRequest>();

    // The server responds with 200.
    ON_CALL(**request, GetHttpStatusCode())
        .WillByDefault(Return(HTTP_STATUS_OK));

    // And a valid response.
    std::vector<uint8> response;
    ASSERT_NO_FATAL_FAILURE(MakeSuccessResponseBody(dm_token, &response));
    ON_CALL(**request, GetResponse()).WillByDefault(Return(response));
  }

 private:
  // Populates |body| with a valid serialized DeviceRegisterResponse.
  // Note: always wrap calls to this with ASSERT_NO_FATAL_FAILURE.
  void MakeSuccessResponseBody(const char* dm_token, std::vector<uint8>* body) {
    enterprise_management::DeviceManagementResponse dm_response;
    dm_response.mutable_register_response()->
        set_device_management_token(dm_token);
    std::string response_string;
    ASSERT_TRUE(dm_response.SerializeToString(&response_string));
    body->assign(response_string.begin(), response_string.end());
  }

  DISALLOW_COPY_AND_ASSIGN(DmClientRequestTest);
};

// Test that DmClient can send a reasonable RegisterBrowserRequest and handle a
// corresponding DeviceRegisterResponse.
TEST_F(DmClientRequestTest, RegisterWithRequest) {
  static const char kDmToken[] = "dm_token";
  static const TCHAR kDeviceId[] = _T("device_id");

  MockHttpRequest* mock_http_request = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeSuccessHttpRequest(kDmToken, &mock_http_request));

  // Expect the proper URL with query params.
  EXPECT_CALL(*mock_http_request,
              set_url(IsValidRequestUrl(_T("register_browser"), kDeviceId)));

  // Expect that the request headers contain the enrollment token.
  EXPECT_CALL(*mock_http_request,
              set_additional_headers(
                  CStringHasSubstr(_T("Authorization: GoogleEnrollmentToken ")
                                   _T("token=enrollment_token"))));

  // Expect that the body of the request contains a well-formed register browser
  // request.
  EXPECT_CALL(*mock_http_request, set_request_buffer(_, _))
      .With(AllArgs(IsRegisterBrowserRequest()));

  // Registration should succeed, providing the expected DMToken.
  CStringA dm_token;
  ASSERT_HRESULT_SUCCEEDED(internal::RegisterWithRequest(mock_http_request,
                                                         _T("enrollment_token"),
                                                         kDeviceId,
                                                         &dm_token));
  EXPECT_STREQ(dm_token.GetString(), kDmToken);
}

class DmClientRegistryTest : public RegistryProtectedTest {
};

TEST_F(DmClientRegistryTest, GetRegistrationState) {
  // No enrollment token.
  {
    DmStorage dm_storage((CString()));
    EXPECT_EQ(GetRegistrationState(&dm_storage), kNotManaged);
  }

  // Enrollment token without device management token.
  {
    DmStorage dm_storage(_T("enrollment_token"));
    EXPECT_EQ(GetRegistrationState(&dm_storage), kRegistrationPending);
  }

  // Enrollment token and device management token.
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken("dm_token"));
  {
    DmStorage dm_storage(_T("enrollment_token"));
    EXPECT_EQ(GetRegistrationState(&dm_storage), kRegistered);
  }

  // Device management token without enrollment token.
  {
    DmStorage dm_storage((CString()));
    EXPECT_EQ(GetRegistrationState(&dm_storage), kRegistered);
  }
}

TEST(DmClientTest, GetAgent) {
  EXPECT_FALSE(internal::GetAgent().IsEmpty());
}

TEST(DmClientTest, GetPlatform) {
  EXPECT_FALSE(internal::GetPlatform().IsEmpty());
}

TEST(DmClientTest, GetOsVersion) {
  EXPECT_FALSE(internal::GetOsVersion().IsEmpty());
}

TEST(DmClientTest, AppendQueryParamsToUrl) {
  static const TCHAR kUrl[] = _T("https://some.net/endpoint");
  std::vector<std::pair<CString,CString>> params;
  params.push_back(std::make_pair(_T("one"), _T("1")));
  params.push_back(std::make_pair(_T("2"), _T("two")));
  CString url(kUrl);
  EXPECT_HRESULT_SUCCEEDED(internal::AppendQueryParamsToUrl(params, &url));
  EXPECT_EQ(url, CString(kUrl) + _T("?one=1&2=two"));
}

TEST(DmClientTest, FormatEnrollmentTokenAuthorizationHeader) {
  static const TCHAR kToken[] = _T("token");
  EXPECT_EQ(internal::FormatEnrollmentTokenAuthorizationHeader(kToken),
            _T("GoogleEnrollmentToken token=token"));
}

}  // namespace dm_client
}  // namespace omaha
