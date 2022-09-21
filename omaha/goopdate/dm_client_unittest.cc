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

#include <stdint.h>
#include <ctime>
#include <iterator>
#include <map>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "crypto/rsa_private_key.h"
#include "crypto/signature_creator.h"
#include "gtest/gtest-matchers.h"
#include "omaha/base/app_util.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/path.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/common/config_manager.h"
#include "omaha/goopdate/dm_storage.h"
#include "omaha/goopdate/dm_storage_test_utils.h"
#include "omaha/net/http_request.h"
#include "omaha/testing/unit_test.h"
#include "wireless/android/enterprise/devicemanagement/proto/dm_api.pb.h"
#include "wireless/android/enterprise/devicemanagement/proto/omaha_settings.pb.h"

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

// Signing private key data (for testing only) in DER-encoded PKCS8 format.
const uint8_t kSigningPrivateKey[] = {
    0x30, 0x82, 0x01, 0x55, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3f, 0x30, 0x82, 0x01, 0x3b, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0xd9, 0xcd, 0xca, 0xcd, 0xc3, 0xea, 0xbe, 0x72, 0x79, 0x1c, 0x29, 0x37,
    0x39, 0x99, 0x1f, 0xd4, 0xb3, 0x0e, 0xf0, 0x7b, 0x78, 0x77, 0x0e, 0x05,
    0x3b, 0x65, 0x34, 0x12, 0x62, 0xaf, 0xa6, 0x8d, 0x33, 0xce, 0x78, 0xf8,
    0x47, 0x05, 0x1d, 0x98, 0xaa, 0x1b, 0x1f, 0x50, 0x05, 0x5b, 0x3c, 0x19,
    0x3f, 0x80, 0x83, 0x63, 0x63, 0x3a, 0xec, 0xcb, 0x2e, 0x90, 0x4f, 0xf5,
    0x26, 0x76, 0xf1, 0xd5, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x64,
    0x29, 0xc2, 0xd9, 0x6b, 0xfe, 0xf9, 0x84, 0x75, 0x73, 0xe0, 0xf4, 0x77,
    0xb5, 0x96, 0xb0, 0xdf, 0x83, 0xc0, 0x4e, 0x57, 0xf1, 0x10, 0x6e, 0x91,
    0x89, 0x12, 0x30, 0x5e, 0x57, 0xff, 0x14, 0x59, 0x5f, 0x18, 0x86, 0x4e,
    0x4b, 0x17, 0x56, 0xfc, 0x8d, 0x40, 0xdd, 0x74, 0x65, 0xd3, 0xff, 0x67,
    0x64, 0xcb, 0x9c, 0xb4, 0x14, 0x8a, 0x06, 0xb7, 0x13, 0x45, 0x94, 0x16,
    0x7d, 0x3f, 0xe1, 0x02, 0x21, 0x00, 0xf6, 0x0f, 0x31, 0x6d, 0x06, 0xcc,
    0x3b, 0xa0, 0x44, 0x1f, 0xf5, 0xc2, 0x45, 0x2b, 0x10, 0x6c, 0xf9, 0x6f,
    0x8f, 0x87, 0x3d, 0xc0, 0x3b, 0x55, 0x13, 0x37, 0x80, 0xcd, 0x9f, 0xe1,
    0xb7, 0xd9, 0x02, 0x21, 0x00, 0xe2, 0x9a, 0x5f, 0xbf, 0x95, 0x74, 0xb5,
    0x7a, 0x6a, 0xa6, 0x97, 0xbd, 0x75, 0x8c, 0x97, 0x18, 0x24, 0xd6, 0x09,
    0xcd, 0xdc, 0xb5, 0x94, 0xbf, 0xe2, 0x78, 0xaa, 0x20, 0x47, 0x9f, 0x68,
    0x5d, 0x02, 0x21, 0x00, 0xaf, 0x8f, 0x97, 0x8c, 0x5a, 0xd5, 0x4d, 0x95,
    0xc4, 0x05, 0xa9, 0xab, 0xba, 0xfe, 0x46, 0xf1, 0xf9, 0xe7, 0x07, 0x59,
    0x4f, 0x4d, 0xe1, 0x07, 0x8a, 0x76, 0x87, 0x88, 0x2f, 0x13, 0x35, 0xc1,
    0x02, 0x20, 0x24, 0xc3, 0xd9, 0x2f, 0x13, 0x47, 0x99, 0x3e, 0x20, 0x59,
    0xa1, 0x1a, 0xeb, 0x1c, 0x81, 0x53, 0x38, 0x7e, 0xc5, 0x9e, 0x71, 0xe5,
    0xc0, 0x19, 0x95, 0xdb, 0xef, 0xf6, 0x46, 0xc8, 0x95, 0x3d, 0x02, 0x21,
    0x00, 0xaa, 0xb1, 0xff, 0x8a, 0xa2, 0xb2, 0x2b, 0xef, 0x9a, 0x83, 0x3f,
    0xc5, 0xbc, 0xd4, 0x6a, 0x07, 0xe8, 0xc7, 0x0b, 0x2e, 0xd4, 0x0f, 0xf8,
    0x98, 0x68, 0xe1, 0x04, 0xa8, 0x92, 0xd0, 0x10, 0xaa,
};

// SHA256 signature of the public key corresponding to private key
// kSigningPrivateKey for "example.com" domain signed with
// kPolicyVerificationKey.
const uint8_t kSigningPublicKeySignature[] = {
    0x97, 0xEB, 0x13, 0xE6, 0x6C, 0xE2, 0x7A, 0x2F, 0xC6, 0x6E, 0x68, 0x8F,
    0xED, 0x5B, 0x51, 0x08, 0x27, 0xF0, 0xA5, 0x97, 0x20, 0xEE, 0xE2, 0x9B,
    0x5B, 0x63, 0xA5, 0x9C, 0xAE, 0x41, 0xFD, 0x34, 0xC4, 0x2E, 0xEB, 0x63,
    0x10, 0x80, 0x0C, 0x74, 0x77, 0x6E, 0x34, 0x1C, 0x1B, 0x3B, 0x8E, 0x2A,
    0x3A, 0x7F, 0xF9, 0x73, 0xB6, 0x2B, 0xB6, 0x45, 0xDB, 0x05, 0xE8, 0x5A,
    0x68, 0x36, 0x05, 0x3C, 0x62, 0x3A, 0x6C, 0x64, 0xDB, 0x0E, 0x61, 0xBD,
    0x29, 0x1C, 0x61, 0x4B, 0xE0, 0xDA, 0x07, 0xBA, 0x29, 0x81, 0xF0, 0x90,
    0x58, 0xB8, 0xBB, 0xF4, 0x69, 0xFF, 0x8F, 0x2B, 0x4A, 0x2D, 0x98, 0x51,
    0x37, 0xF5, 0x52, 0xCB, 0xE3, 0xC4, 0x6D, 0xEC, 0xEA, 0x32, 0x2D, 0xDD,
    0xD7, 0xFC, 0x43, 0xC6, 0x54, 0xE1, 0xC1, 0x66, 0x43, 0x37, 0x09, 0xE1,
    0xBF, 0xD1, 0x11, 0xFC, 0xDB, 0xBF, 0xDF, 0x66, 0x53, 0x8F, 0x38, 0x2D,
    0xAA, 0x89, 0xD2, 0x9F, 0x60, 0x90, 0xB7, 0x05, 0xC2, 0x20, 0x82, 0xE6,
    0xE0, 0x57, 0x55, 0xFF, 0x5F, 0xC1, 0x76, 0x66, 0x46, 0xF8, 0x67, 0xB8,
    0x8B, 0x81, 0x53, 0xA9, 0x8B, 0x48, 0x9E, 0x2A, 0xF9, 0x60, 0x57, 0xBA,
    0xD7, 0x52, 0x97, 0x53, 0xF0, 0x2F, 0x78, 0x68, 0x50, 0x18, 0x12, 0x00,
    0x5E, 0x8E, 0x2A, 0x62, 0x0D, 0x48, 0xA9, 0xB5, 0x6B, 0xBC, 0xA0, 0x52,
    0x53, 0xD7, 0x65, 0x23, 0xA4, 0xA5, 0xF5, 0x32, 0x49, 0x2D, 0xB2, 0x77,
    0x2C, 0x66, 0x97, 0xBA, 0x58, 0xE0, 0x16, 0x1C, 0x8C, 0x02, 0x5D, 0xE0,
    0x73, 0x2E, 0xDF, 0xB4, 0x2F, 0x4C, 0xA2, 0x11, 0x26, 0xC1, 0xAF, 0xAC,
    0x73, 0xBC, 0xB6, 0x98, 0xE0, 0x20, 0x61, 0x0E, 0x52, 0x4A, 0x6C, 0x80,
    0xB5, 0x0C, 0x10, 0x80, 0x09, 0x17, 0xF4, 0x9D, 0xFE, 0xB5, 0xFC, 0x63,
    0x9A, 0x80, 0x3F, 0x76,
};

// New signing private key data (for testing only) in DER-encoded PKCS8 format.
const uint8_t kNewSigningPrivateKey[] = {
    0x30, 0x82, 0x01, 0x54, 0x02, 0x01, 0x00, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x04, 0x82,
    0x01, 0x3e, 0x30, 0x82, 0x01, 0x3a, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00,
    0x99, 0x98, 0x6b, 0x79, 0x5d, 0x38, 0x33, 0x79, 0x27, 0x0a, 0x2e, 0xb0,
    0x89, 0xba, 0xf8, 0xf6, 0x80, 0xde, 0xb0, 0x79, 0xf2, 0xd4, 0x6d, 0xf7,
    0x3c, 0xa3, 0x97, 0xf6, 0x4a, 0x3c, 0xa5, 0xcc, 0x40, 0x8a, 0xef, 0x59,
    0xaa, 0xc2, 0x82, 0x8f, 0xbc, 0x0d, 0x5b, 0x63, 0xc6, 0xaa, 0x72, 0xe2,
    0xf3, 0x57, 0xdd, 0x74, 0x00, 0xb0, 0x42, 0xd6, 0x27, 0xe7, 0x17, 0x61,
    0x0a, 0xdc, 0xc1, 0xf7, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x40, 0x34,
    0xcf, 0xc9, 0xb4, 0x73, 0x2f, 0x0d, 0xd3, 0xcc, 0x6e, 0x9d, 0xdb, 0x29,
    0xa0, 0x56, 0x56, 0x3b, 0xbd, 0x56, 0x24, 0xb8, 0x2f, 0xfe, 0x97, 0x92,
    0x0c, 0x16, 0x06, 0x23, 0x44, 0x73, 0x25, 0x1d, 0x65, 0xf4, 0xda, 0x77,
    0xe7, 0x91, 0x2e, 0x91, 0x05, 0x10, 0xc1, 0x1b, 0x39, 0x5e, 0xb2, 0xf7,
    0xbd, 0x14, 0x19, 0xcb, 0x6b, 0xc3, 0xa9, 0xe8, 0x91, 0xf7, 0xa7, 0xa9,
    0x90, 0x08, 0x51, 0x02, 0x21, 0x00, 0xcc, 0x9e, 0x03, 0x54, 0x8f, 0x24,
    0xde, 0x90, 0x25, 0xec, 0x21, 0xaf, 0xe6, 0x27, 0x2a, 0x16, 0x42, 0x74,
    0xda, 0xf8, 0x84, 0xc4, 0x8c, 0x1e, 0x86, 0x12, 0x04, 0x5c, 0x17, 0x01,
    0xea, 0x9d, 0x02, 0x21, 0x00, 0xc0, 0x2a, 0x6c, 0xe9, 0xa1, 0x1a, 0x41,
    0x11, 0x94, 0x50, 0xf7, 0x1a, 0xd3, 0xbc, 0xf3, 0xa2, 0xf8, 0x46, 0xbc,
    0x26, 0x77, 0x78, 0xef, 0xc0, 0x54, 0xec, 0x22, 0x3f, 0x2c, 0x57, 0xe0,
    0xa3, 0x02, 0x20, 0x31, 0xf2, 0xc8, 0xa1, 0x55, 0xa8, 0x0c, 0x64, 0x67,
    0xbd, 0x72, 0xa3, 0xbb, 0xad, 0x07, 0xcb, 0x13, 0x41, 0xef, 0x4a, 0x07,
    0x2e, 0xeb, 0x7d, 0x70, 0x00, 0xe9, 0xeb, 0x88, 0xfa, 0x40, 0xc9, 0x02,
    0x20, 0x3a, 0xe0, 0xc4, 0xde, 0x10, 0x6e, 0x6a, 0xe1, 0x68, 0x00, 0x26,
    0xb6, 0x21, 0x8a, 0x13, 0x5c, 0x2b, 0x96, 0x00, 0xb0, 0x08, 0x8b, 0x15,
    0x6a, 0x68, 0x9a, 0xb1, 0x23, 0x8a, 0x02, 0xa2, 0xe1, 0x02, 0x21, 0x00,
    0xa3, 0xf2, 0x2d, 0x55, 0xc1, 0x6d, 0x40, 0xfa, 0x1d, 0xf7, 0xba, 0x86,
    0xef, 0x50, 0x98, 0xfc, 0xee, 0x09, 0xcc, 0xe7, 0x22, 0xb9, 0x4e, 0x80,
    0x32, 0x1a, 0x6b, 0xb3, 0x5f, 0x35, 0xbd, 0xf3,
};

// SHA256 signature of the public key corresponding to private key
// kNewSigningPrivateKey for "example.com" domain signed with
// kPolicyVerificationKey.
const uint8_t kNewSigningPublicKeySignature[] = {
    0x70, 0xED, 0x27, 0x42, 0x34, 0x69, 0xB6, 0x47, 0x9E, 0x7C, 0xA0, 0xF0,
    0xE5, 0x0A, 0x49, 0x49, 0x00, 0xDA, 0xBC, 0x70, 0x01, 0xC5, 0x4B, 0xDB,
    0x47, 0xD5, 0xAF, 0xA1, 0xAD, 0xB7, 0xE4, 0xE1, 0xBD, 0x5A, 0x1C, 0x35,
    0x44, 0x5A, 0xAA, 0xDB, 0x27, 0xBA, 0xA4, 0xA9, 0xC8, 0xDD, 0xEC, 0xD6,
    0xEB, 0xFE, 0xDB, 0xE0, 0x03, 0x5C, 0xA6, 0x2E, 0x5A, 0xEC, 0x75, 0x79,
    0xB8, 0x5F, 0x0A, 0xEE, 0x05, 0xB2, 0x61, 0xDC, 0x58, 0xF0, 0xD1, 0xCB,
    0x7B, 0x2A, 0xDB, 0xC1, 0x7C, 0x60, 0xE6, 0x3E, 0x87, 0x02, 0x61, 0xE6,
    0x90, 0xFD, 0x54, 0x65, 0xC7, 0xFF, 0x74, 0x09, 0xD6, 0xAA, 0x8E, 0xDC,
    0x5B, 0xC8, 0x38, 0x0C, 0x84, 0x0E, 0x84, 0x2E, 0x37, 0x2A, 0x4B, 0xDE,
    0x31, 0x82, 0x76, 0x1E, 0x77, 0xA5, 0xC1, 0xD5, 0xED, 0xFF, 0xBC, 0xEA,
    0x91, 0xB7, 0xBC, 0xFF, 0x76, 0x23, 0xE2, 0x78, 0x63, 0x01, 0x47, 0x80,
    0x47, 0x1F, 0x3A, 0x49, 0xBF, 0x0D, 0xCF, 0x27, 0x70, 0x92, 0xBB, 0xEA,
    0xB3, 0x92, 0x70, 0xFF, 0x1E, 0x4B, 0x1B, 0xE0, 0x4E, 0x0C, 0x4C, 0x6B,
    0x5D, 0x77, 0x06, 0xBB, 0xFB, 0x9B, 0x0E, 0x55, 0xB8, 0x8A, 0xF2, 0x45,
    0xA9, 0xF3, 0x54, 0x3D, 0x0C, 0xAC, 0xA8, 0x15, 0xD2, 0x31, 0x8D, 0x97,
    0x08, 0x73, 0xC9, 0x0F, 0x1D, 0xDE, 0x10, 0x22, 0xC6, 0x55, 0x53, 0x7F,
    0x7C, 0x50, 0x16, 0x5A, 0x08, 0xCC, 0x1C, 0x53, 0x9B, 0x02, 0xB8, 0x80,
    0xB7, 0x46, 0xF5, 0xF1, 0xC7, 0x3D, 0x36, 0xBD, 0x26, 0x02, 0xDE, 0x10,
    0xAB, 0x5A, 0x03, 0xCD, 0x67, 0x00, 0x1C, 0x23, 0xC7, 0x13, 0xEE, 0x5D,
    0xAF, 0xC5, 0x1F, 0xE3, 0xA0, 0x54, 0xAC, 0xC2, 0xC9, 0x44, 0xD4, 0x4A,
    0x09, 0x8E, 0xEB, 0xAE, 0xCA, 0x08, 0x8A, 0x7F, 0x41, 0x7B, 0xD8, 0x2C,
    0xDD, 0x6F, 0x80, 0xC3,
};

const char kDomain[] = "example.com";
const char kUsername[] = "username@example.com";
const char kDmToken[] = "dm_token";
const TCHAR kDeviceId[] = _T("device_id");

// A Google Mock matcher that returns true if a string contains a valid
// URL to the device management server with the required query parameters.
class IsValidRequestUrlMatcher
    : public ::testing::MatcherInterface<const CString&> {
 public:
  IsValidRequestUrlMatcher(
      std::vector<std::pair<CString, CString>> query_params)
      : query_params_(std::move(query_params)) {
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
    std::map<CString, CString> query_params;
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
    for (const auto& query_param : query_params_) {
      const TCHAR* p = query_param.first;
      if (query_params.find(p) == query_params.end()) {
        *listener << "the url is missing the \"" << WideToUtf8(p)
                  << "\" query parameter";
        return false;
      }

      CString expected_param_value;
      HRESULT hr = StringEscape(query_param.second,
                                false,
                                &expected_param_value);
      if (FAILED(hr)) {
        *listener << "failed to StringEscape \""
                  << WideToUtf8(query_param.second)
                  << "\" query parameter";
        return false;
      }

      if (query_params[p] != expected_param_value) {
        *listener << "the actual request query parameter is \""
                  << WideToUtf8(query_params[p]) << "\""
                  << " and does not match the expected query parameter of \""
                  << WideToUtf8(expected_param_value) << "\"";
        return false;
      }
    }

    return true;
  }

  virtual void DescribeTo(std::ostream* os) const {
    *os << "string contains a valid device management request URL";
  }

 private:
  const std::vector<std::pair<CString, CString>> query_params_;
  CString device_management_url_;
};

// Returns an IsValidRequestUrl matcher, which takes a CString and matches if
// it is an URL leading to the device management server endpoint, and contains
// all the required query parameters in |query_params|.
::testing::Matcher<const CString&> IsValidRequestUrl(
    std::vector<std::pair<CString, CString>> query_params) {
  return ::testing::MakeMatcher(
      new IsValidRequestUrlMatcher(std::move(query_params)));
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

  virtual void DescribeTo(std::ostream* os) const {
    *os << "buffer contains a valid serialized RegisterBrowserRequest";
  }
};

// A Google Mock matcher that returns true if a buffer contains a valid
// serialized PolicyValidationReportRequest message. Field values are checked
// against the expected values when possible.
class IsPolicyValidationReportRequestMatcher
    : public ::testing::MatcherInterface<
          const ::testing::tuple<const void*, size_t>&> {
 public:
  explicit IsPolicyValidationReportRequestMatcher(
      const PolicyValidationResult& expected_validation_result)
      : expected_validation_result_(expected_validation_result) {}
  virtual bool MatchAndExplain(
      const ::testing::tuple<const void*, size_t>& buffer,
      ::testing::MatchResultListener* listener) const {
    enterprise_management::DeviceManagementRequest request;
    if (!request.ParseFromArray(::testing::get<0>(buffer),
                                static_cast<int>(::testing::get<1>(buffer)))) {
      *listener << "parse failure";
      return false;
    }
    if (!request.has_policy_validation_report_request()) {
      *listener << "missing policy_validation_report_request";
      return false;
    }
    const enterprise_management::PolicyValidationReportRequest&
        error_report_request = request.policy_validation_report_request();
    if (!error_report_request.has_validation_result_type()) {
      *listener << "unexpected error_report_request.validation_result_type";
      return false;
    }
    if (!error_report_request.has_policy_type() ||
        error_report_request.policy_type() !=
            expected_validation_result_.policy_type) {
      *listener << "unexpected error_report_request.policy_type";
      return false;
    }
    if (!error_report_request.has_policy_token() ||
        error_report_request.policy_token() !=
            expected_validation_result_.policy_token) {
      *listener << "missing error_report_request.policy_token";
      return false;
    }
    if (expected_validation_result_.issues.size() !=
        error_report_request.policy_value_validation_issues_size()) {
      *listener << "unexpected number of issues in error_report_request";
      return false;
    }

    for (size_t i = 0; i < expected_validation_result_.issues.size(); ++i) {
      auto expected_issue = expected_validation_result_.issues[i];
      auto issue_in_request =
          error_report_request.policy_value_validation_issues(i);
      if (!issue_in_request.has_policy_name() ||
          issue_in_request.policy_name() != expected_issue.policy_name) {
        *listener << "unexpected issue policy name";
        return false;
      }
      if (!issue_in_request.has_severity()) {
        *listener << "unexpected issue severity";
        return false;
      }
      if (!issue_in_request.has_debug_message() ||
          issue_in_request.debug_message() != expected_issue.message) {
        *listener << "unexpected issue message";
        return false;
      }
    }

    return true;
  }

  virtual void DescribeTo(std::ostream* os) const {
    *os << "buffer contains a valid serialized PolicyValidationReportRequest";
  }

 private:
  const PolicyValidationResult expected_validation_result_;
};

// A Google Mock matcher that returns true if a buffer contains a valid
// serialized DevicePolicyRequest message. While the presence of each field
// in the request is checked, the exact value of each is not.
class IsFetchPoliciesRequestMatcher
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
    if (!request.has_policy_request()) {
      *listener << "missing policy_request";
      return false;
    }
    if (!request.policy_request().requests_size()) {
      *listener << "unexpected requests_size() == 0";
      return false;
    }
    const enterprise_management::PolicyFetchRequest& policy_request =
        request.policy_request().requests(0);
    if (!policy_request.has_policy_type()) {
      *listener << "missing policy_request.has_policy_type";
      return false;
    }
    if (!policy_request.has_signature_type()) {
      *listener << "missing policy_request.has_signature_type";
      return false;
    }
    if (!policy_request.has_verification_key_hash()) {
      *listener << "missing policy_request.has_verification_key_hash";
      return false;
    }
    return true;
  }


  virtual void DescribeTo(std::ostream* os) const {
    *os << "buffer contains a valid serialized DevicePolicyRequest";
  }
};

// Returns an IsRegisterBrowserRequest matcher, which takes a tuple of a pointer
// to a buffer and a buffer size.
::testing::Matcher<const ::testing::tuple<const void*, size_t>& >
IsRegisterBrowserRequest() {
  return ::testing::MakeMatcher(new IsRegisterBrowserRequestMatcher);
}

// Returns an IsPolicyValidationReportRequest matcher, which takes a tuple of a
// pointer to a buffer and a buffer size.
::testing::Matcher<const ::testing::tuple<const void*, size_t>&>
IsPolicyValidationReportRequest(
    const PolicyValidationResult& expected_validation_result) {
  return ::testing::MakeMatcher(
      new IsPolicyValidationReportRequestMatcher(expected_validation_result));
}

// Returns an IsFetchPoliciesRequest matcher, which takes a tuple of a pointer
// to a buffer and a buffer size.
::testing::Matcher<const ::testing::tuple<const void*, size_t>& >
IsFetchPoliciesRequest() {
  return ::testing::MakeMatcher(new IsFetchPoliciesRequestMatcher);
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
  // successfully processed a HTTP request, returning a HTTP response containing
  // |response_data|.
  // Note: always wrap calls to this with ASSERT_NO_FATAL_FAILURE.
  template <typename T>
  void MakeSuccessHttpRequest(T response_data, MockHttpRequest** request) {
    *request = new ::testing::NiceMock<MockHttpRequest>();

    // The server responds with 200.
    ON_CALL(**request, GetHttpStatusCode())
        .WillByDefault(Return(HTTP_STATUS_OK));

    // And a valid response.
    std::vector<uint8> response;
    ASSERT_NO_FATAL_FAILURE(MakeSuccessResponseBody(response_data, &response));
    ON_CALL(**request, GetResponse()).WillByDefault(Return(response));
  }

  // Populates |request| with a mock HttpRequest that behaves as if the server
  // returned a 410 HTTP response.
  // Note: always wrap calls to this with ASSERT_NO_FATAL_FAILURE.
  void MakeGoneHttpRequest(bool delete_dm_token, MockHttpRequest** request) {
    *request = new ::testing::NiceMock<MockHttpRequest>();

    // The server responds with 410.
    ON_CALL(**request, GetHttpStatusCode())
        .WillByDefault(Return(HTTP_STATUS_GONE));

    // And response data guiding how to update DM token.
    std::vector<uint8> response;
    ASSERT_NO_FATAL_FAILURE(
        MakeHTTPGoneResponseBody(delete_dm_token, &response));
    ON_CALL(**request, GetResponse()).WillByDefault(Return(response));
  }

  struct KeyInfo {
    const std::unique_ptr<crypto::RSAPrivateKey>& signing_private_key;
    const std::string& signing_public_key;
    const std::string& signing_public_key_signature;
    const std::unique_ptr<crypto::RSAPrivateKey>& cached_signing_private_key;
  };

  struct FetchPoliciesInput {
    const std::unique_ptr<KeyInfo>& key_info;
    const PolicyResponses& responses;
  };

  void RunFetchPolicies(const std::unique_ptr<KeyInfo>& key_info,
                        CachedPolicyInfo* info) {
    ASSERT_TRUE(info);

    PolicyResponsesMap expected_responses = {
        {"google/chrome/machine-level-user", "test-data-chrome"},
        {"google/drive/machine-level-user", "test-data-drive"},
        {"google/earth/machine-level-user", "test-data-earth"},
    };

    enterprise_management::PublicKeyVerificationData signed_data;
    signed_data.set_new_public_key(key_info->signing_public_key);
    signed_data.set_domain(kDomain);

    PolicyResponses input = {expected_responses,
                             signed_data.SerializeAsString()};
    FetchPoliciesInput fetch_input = {key_info, input};
    MockHttpRequest* mock_http_request = nullptr;
    ASSERT_NO_FATAL_FAILURE(MakeSuccessHttpRequest(fetch_input,
                                                   &mock_http_request));

    const std::vector<std::pair<CString, CString>> query_params = {
        {_T("request"), _T("policy")},
        {_T("agent"), internal::GetAgent()},
        {_T("apptype"), _T("Chrome")},
        {_T("deviceid"), kDeviceId},
        {_T("platform"), internal::GetPlatform()},
    };

    // Expect the proper URL with query params.
    EXPECT_CALL(*mock_http_request,
                set_url(IsValidRequestUrl(std::move(query_params))));

    // Expect that the request headers contain the DMToken.
    EXPECT_CALL(*mock_http_request,
                set_additional_headers(
                    CStringHasSubstr(_T("Content-Type: application/protobuf")
                                     _T("\r\nAuthorization: GoogleDMToken ")
                                     _T("token=dm_token"))));

    // Expect that the body of the request contains a well-formed fetch policies
    // request.
    EXPECT_CALL(*mock_http_request, set_request_buffer(_, _))
        .With(AllArgs(IsFetchPoliciesRequest()));

    EXPECT_HRESULT_SUCCEEDED(DmStorage::CreateInstance(CString()));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    DmStorage::Instance()->StoreDmToken(kDmToken);

    // Fetch Policies should succeed, providing the expected PolicyResponses.
    PolicyResponses responses;
    ASSERT_HRESULT_SUCCEEDED(internal::FetchPolicies(
        DmStorage::Instance(),
        std::move(std::unique_ptr<HttpRequestInterface>(mock_http_request)),
        CString(kDmToken), kDeviceId, *info, &responses));
    ASSERT_TRUE(!responses.policy_info.empty());
    ASSERT_HRESULT_SUCCEEDED(GetCachedPolicyInfo(responses.policy_info, info));

    EXPECT_EQ(expected_responses.size(), responses.responses.size());
    for (const auto& expected_response : expected_responses) {
      enterprise_management::PolicyFetchResponse response;
      const std::string& string_response =
          responses.responses[expected_response.first.c_str()];
      EXPECT_TRUE(response.ParseFromString(string_response));

      enterprise_management::PolicyData policy_data;
      EXPECT_TRUE(policy_data.ParseFromString(response.policy_data()));
      EXPECT_TRUE(policy_data.IsInitialized());
      EXPECT_TRUE(policy_data.has_policy_type());

      EXPECT_STREQ(expected_response.first.c_str(),
                   policy_data.policy_type().c_str());
      EXPECT_STREQ(expected_response.second.c_str(),
                   policy_data.policy_value().c_str());
    }
  }

  void DecodeOmahaPolicies() {
    wireless_android_enterprise_devicemanagement::ApplicationSettings app;
    app.set_app_guid(CStringA(kChromeAppId));
    wireless_android_enterprise_devicemanagement::OmahaSettingsClientProto
        omaha_settings;
    auto repeated_app_settings = omaha_settings.mutable_application_settings();
    repeated_app_settings->Add(std::move(app));

    enterprise_management::PolicyData policy_data;
    policy_data.set_policy_value(omaha_settings.SerializeAsString());

    enterprise_management::PolicyFetchResponse response;
    response.set_policy_data(policy_data.SerializeAsString());

    CachedOmahaPolicy op;
    ASSERT_HRESULT_SUCCEEDED(GetCachedOmahaPolicy(response.SerializeAsString(),
                                                  &op));
    ASSERT_EQ(op.application_settings.size(), 1);
    ASSERT_TRUE(::IsEqualGUID(op.application_settings.begin()->first,
                StringToGuid(kChromeAppId)));
  }

  std::unique_ptr<crypto::RSAPrivateKey> CreateKey(
      const uint8_t* private_key,
      size_t private_key_length,
      std::string* rsa_public_key) {
    std::vector<uint8_t> input(private_key, private_key + private_key_length);
    std::unique_ptr<crypto::RSAPrivateKey> rsa_private_key(
        crypto::RSAPrivateKey::CreateFromPrivateKeyInfo(input));

    if (rsa_public_key) {
      std::vector<uint8_t> public_key;
      VERIFY1(rsa_private_key->ExportPublicKey(&public_key));

      *rsa_public_key = std::string(
          reinterpret_cast<const char*>(public_key.data()),
          public_key.size());
    }

    return std::move(rsa_private_key);
  }

 private:
  // Produces |key|'s signature over |data| and stores it in |signature|.
  void SignData(const std::string& data,
                crypto::RSAPrivateKey* key,
                std::string* signature) {
    std::unique_ptr<crypto::SignatureCreator> signature_creator(
        crypto::SignatureCreator::Create(key, CALG_SHA1));
    ASSERT_TRUE(signature_creator->Update(
        reinterpret_cast<const uint8_t*>(data.c_str()), data.size()));
    std::vector<uint8_t> signature_bytes;
    ASSERT_TRUE(signature_creator->Final(&signature_bytes));
    signature->assign(reinterpret_cast<const char*>(signature_bytes.data()),
                      signature_bytes.size());
  }

  void SignPolicyResponse(enterprise_management::PolicyFetchResponse* response,
                          const std::unique_ptr<KeyInfo>& key_info) {
    ASSERT_TRUE(response);
    ASSERT_TRUE(key_info->signing_private_key.get());

    // Add the new public key and the corresponding verification key signature
    // to the policy response.
    response->set_new_public_key(key_info->signing_public_key);
    response->set_new_public_key_verification_data_signature(
        key_info->signing_public_key_signature);

    enterprise_management::PublicKeyVerificationData signed_data;
    signed_data.set_new_public_key(key_info->signing_public_key);
    signed_data.set_domain(kDomain);
    response->set_new_public_key_verification_data(
        signed_data.SerializeAsString());

    // Add the PolicyData signature to the policy response.
    SignData(response->policy_data(),
             key_info->signing_private_key.get(),
             response->mutable_policy_data_signature());

    if (key_info->cached_signing_private_key.get()) {
      // Use the cached private key to sign the new public key and add the
      // signature to the policy response.
      SignData(response->new_public_key(),
               key_info->cached_signing_private_key.get(),
               response->mutable_new_public_key_signature());
    }
  }

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

  // Populates |body| with a valid serialized DevicePolicyResponse.
  // Note: always wrap calls to this with ASSERT_NO_FATAL_FAILURE.
  void MakeSuccessResponseBody(const FetchPoliciesInput& fetch_input,
                               std::vector<uint8>* body) {
    const PolicyResponses& input = fetch_input.responses;
    const PolicyResponsesMap& responses = input.responses;

    enterprise_management::DeviceManagementResponse dm_response;

    for (const auto& response : responses) {
      enterprise_management::PolicyFetchResponse* policy_response =
          dm_response.mutable_policy_response()->add_responses();
      enterprise_management::PolicyData policy_data;
      policy_data.set_policy_type(response.first);
      policy_data.set_policy_value(response.second);
      policy_data.set_username(kUsername);
      policy_data.set_request_token(kDmToken);
      policy_data.set_device_id(CStringA(kDeviceId));
      policy_data.set_timestamp(time(NULL));
      policy_response->set_policy_data(policy_data.SerializeAsString());
      SignPolicyResponse(policy_response, fetch_input.key_info);
    }

    std::string response_string;
    ASSERT_TRUE(dm_response.SerializeToString(&response_string));
    body->assign(response_string.begin(), response_string.end());
  }

  // Populates |body| with a serialized DevicePolicyResponse for HTTP Gone.
  // Note: always wrap calls to this with ASSERT_NO_FATAL_FAILURE.
  void MakeHTTPGoneResponseBody(bool delete_dm_token,
                                std::vector<uint8>* body) {
    if (!delete_dm_token) return;

    enterprise_management::DeviceManagementResponse dm_response;
    dm_response.add_error_detail(
        enterprise_management::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN);
    std::string response_string;
    ASSERT_TRUE(dm_response.SerializeToString(&response_string));
    body->assign(response_string.begin(), response_string.end());
  }

  DISALLOW_COPY_AND_ASSIGN(DmClientRequestTest);
};

// Test that DmClient can send a reasonable RegisterBrowserRequest and handle a
// corresponding DeviceRegisterResponse.
TEST_F(DmClientRequestTest, RegisterWithRequest) {
  MockHttpRequest* mock_http_request = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeSuccessHttpRequest(kDmToken, &mock_http_request));

  const std::vector<std::pair<CString, CString>> query_params = {
      {_T("request"), _T("register_policy_agent")},
      {_T("agent"), internal::GetAgent()},
      {_T("apptype"), _T("Chrome")},
      {_T("deviceid"), kDeviceId},
      {_T("platform"), internal::GetPlatform()},
  };

  // Expect the proper URL with query params.
  EXPECT_CALL(*mock_http_request,
              set_url(IsValidRequestUrl(std::move(query_params))));

  // Expect that the request headers contain the enrollment token.
  EXPECT_CALL(*mock_http_request,
              set_additional_headers(
                  CStringHasSubstr(_T("Content-Type: application/protobuf\r\n")
                                   _T("Authorization: GoogleEnrollmentToken ")
                                   _T("token=57FEBE8F-48D0-487B-A788-")
                                   _T("CF1019DCD452"))));

  // Expect that the body of the request contains a well-formed register browser
  // request.
  EXPECT_CALL(*mock_http_request, set_request_buffer(_, _))
      .With(AllArgs(IsRegisterBrowserRequest()));

  // Registration should succeed, providing the expected DMToken.
  EXPECT_HRESULT_SUCCEEDED(DmStorage::CreateInstance(CString()));
  ON_SCOPE_EXIT(DmStorage::DeleteInstance);

  // Test successful registration.
  ASSERT_HRESULT_SUCCEEDED(internal::RegisterWithRequest(
      DmStorage::Instance(),
      std::move(std::unique_ptr<HttpRequestInterface>(mock_http_request)),
      _T("57FEBE8F-48D0-487B-A788-CF1019DCD452"), kDeviceId));
  EXPECT_EQ(DmStorage::Instance()->GetDmToken(), kDmToken);

  // Test DM Token deletion.
  MockHttpRequest* mock_gone_request = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeGoneHttpRequest(true, &mock_gone_request));
  ASSERT_EQ(
      internal::RegisterWithRequest(
          DmStorage::Instance(),
          std::move(std::unique_ptr<HttpRequestInterface>(mock_gone_request)),
          _T("57FEBE8F-48D0-487B-A788-CF1019DCD452"), kDeviceId),
      HRESULTFromHttpStatusCode(HTTP_STATUS_GONE));
  EXPECT_TRUE(DmStorage::Instance()->GetDmToken().IsEmpty());

  // Test DM Token invalidation.
  mock_gone_request = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeGoneHttpRequest(false, &mock_gone_request));
  ASSERT_EQ(
      internal::RegisterWithRequest(
          DmStorage::Instance(),
          std::move(std::unique_ptr<HttpRequestInterface>(mock_gone_request)),
          _T("57FEBE8F-48D0-487B-A788-CF1019DCD452"), kDeviceId),
      HRESULTFromHttpStatusCode(HTTP_STATUS_GONE));
  EXPECT_TRUE(DmStorage::Instance()->IsInvalidDMToken());
}

TEST_F(DmClientRequestTest, SendPolicyValidationResultReportIfNeeded) {
  PolicyValidationResult validation_result;
  validation_result.policy_type = "google/chrome/machine-level-user";
  validation_result.policy_token = "some_token";
  validation_result.status =
      PolicyValidationResult::Status::kValidationBadSignature;
  validation_result.issues.push_back(
      {"test_policy1", PolicyValueValidationIssue::Severity::kError,
       "test_policy1 value has error"});
  validation_result.issues.push_back(
      {"test_policy2", PolicyValueValidationIssue::Severity::kWarning,
       "test_policy2 value has warning"});

  MockHttpRequest* mock_http_request = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeSuccessHttpRequest("", &mock_http_request));

  const std::vector<std::pair<CString, CString>> query_params = {
      {_T("request"), _T("policy_validation_report")},
      {_T("agent"), internal::GetAgent()},
      {_T("apptype"), _T("Chrome")},
      {_T("deviceid"), kDeviceId},
      {_T("platform"), internal::GetPlatform()},
  };
  EXPECT_CALL(*mock_http_request,
              set_url(IsValidRequestUrl(std::move(query_params))));

  // Expect that the request headers contain the DMToken.
  EXPECT_CALL(*mock_http_request, set_additional_headers(CStringHasSubstr(
                                      _T("Content-Type: application/protobuf")
                                      _T("\r\nAuthorization: GoogleDMToken ")
                                      _T("token=dm_token"))));

  // Expect that the body of the request contains a well-formed policy
  // validation result report request.
  EXPECT_CALL(*mock_http_request, set_request_buffer(_, _))
      .With(AllArgs(IsPolicyValidationReportRequest(validation_result)));

  internal::SendPolicyValidationResultReportIfNeeded(
      std::move(std::unique_ptr<HttpRequestInterface>(mock_http_request)),
      CString(kDmToken), CString(kDeviceId), validation_result);
}

// Test that DmClient can send a reasonable DevicePolicyRequest and handle a
// corresponding DevicePolicyResponse.
TEST_F(DmClientRequestTest, FetchPolicies) {
  std::string signing_public_key;
  const std::unique_ptr<crypto::RSAPrivateKey> signing_private_key(
      std::move(CreateKey(kSigningPrivateKey,
                          sizeof(kSigningPrivateKey),
                          &signing_public_key)));
  const std::string signing_public_key_signature(
      reinterpret_cast<const char*>(kSigningPublicKeySignature),
      sizeof(kSigningPublicKeySignature));
  const std::unique_ptr<KeyInfo> signing_key_info(
      std::make_unique<KeyInfo>(KeyInfo{signing_private_key,
                                        signing_public_key,
                                        signing_public_key_signature,
                                        nullptr}));

  std::string new_signing_public_key;
  const std::unique_ptr<crypto::RSAPrivateKey> new_signing_private_key(
      std::move(CreateKey(kNewSigningPrivateKey,
                          sizeof(kNewSigningPrivateKey),
                          &new_signing_public_key)));
  const std::string new_signing_public_key_signature(
      reinterpret_cast<const char*>(kNewSigningPublicKeySignature),
      sizeof(kNewSigningPublicKeySignature));
  const std::unique_ptr<KeyInfo> new_signing_key_info(
      std::make_unique<KeyInfo>(KeyInfo{new_signing_private_key,
                                        new_signing_public_key,
                                        new_signing_public_key_signature,
                                        signing_private_key}));

  CachedPolicyInfo info;

  // The mock server will return PolicyData signed with kSigningPrivateKey, and
  // a new public key corresponding to kSigningPrivateKey, signed with the
  // hard-coded verification key.
  RunFetchPolicies(signing_key_info, &info);

  // |info| was initialized in the previous run with the signing public key. Now
  // the mock server will return PolicyData signed with kNewSigningPrivateKey,
  // and a new public key corresponding to kNewSigningPrivateKey, signed with
  // both the hard-coded verification key as well as with the previous private
  // key kSigningPrivateKey.
  RunFetchPolicies(new_signing_key_info, &info);

  // Test DM Token invalidation.
  EXPECT_HRESULT_SUCCEEDED(DmStorage::CreateInstance(CString()));
  ON_SCOPE_EXIT(DmStorage::DeleteInstance);
  DmStorage::Instance()->StoreDmToken(kDmToken);
  MockHttpRequest* mock_gone_request = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeGoneHttpRequest(false, &mock_gone_request));

  // Fetch Policies should fail.
  PolicyResponses responses;
  ASSERT_EQ(
      internal::FetchPolicies(
          DmStorage::Instance(),
          std::move(std::unique_ptr<HttpRequestInterface>(mock_gone_request)),
          CString(kDmToken), kDeviceId, info, &responses),
      HRESULTFromHttpStatusCode(HTTP_STATUS_GONE));
  EXPECT_TRUE(DmStorage::Instance()->IsInvalidDMToken());
}

// Test that DmClient can delete DM token per server request.
TEST_F(DmClientRequestTest, DmTokenDeletionInPolicyFetch) {
  std::string signing_public_key;
  const std::unique_ptr<crypto::RSAPrivateKey> signing_private_key(
      std::move(CreateKey(kSigningPrivateKey, sizeof(kSigningPrivateKey),
                          &signing_public_key)));
  const std::string signing_public_key_signature(
      reinterpret_cast<const char*>(kSigningPublicKeySignature),
      sizeof(kSigningPublicKeySignature));
  const std::unique_ptr<KeyInfo> signing_key_info(std::make_unique<KeyInfo>(
      KeyInfo{signing_private_key, signing_public_key,
              signing_public_key_signature, nullptr}));

  std::string new_signing_public_key;
  const std::unique_ptr<crypto::RSAPrivateKey> new_signing_private_key(
      std::move(CreateKey(kNewSigningPrivateKey, sizeof(kNewSigningPrivateKey),
                          &new_signing_public_key)));
  const std::string new_signing_public_key_signature(
      reinterpret_cast<const char*>(kNewSigningPublicKeySignature),
      sizeof(kNewSigningPublicKeySignature));
  const std::unique_ptr<KeyInfo> new_signing_key_info(std::make_unique<KeyInfo>(
      KeyInfo{new_signing_private_key, new_signing_public_key,
              new_signing_public_key_signature, signing_private_key}));

  CachedPolicyInfo info;

  // The mock server will return PolicyData signed with kSigningPrivateKey, and
  // a new public key corresponding to kSigningPrivateKey, signed with the
  // hard-coded verification key.
  RunFetchPolicies(signing_key_info, &info);

  // |info| was initialized in the previous run with the signing public key. Now
  // the mock server will return PolicyData signed with kNewSigningPrivateKey,
  // and a new public key corresponding to kNewSigningPrivateKey, signed with
  // both the hard-coded verification key as well as with the previous private
  // key kSigningPrivateKey.
  RunFetchPolicies(new_signing_key_info, &info);

  // Test DM Token deletion.
  EXPECT_HRESULT_SUCCEEDED(DmStorage::CreateInstance(CString()));
  ON_SCOPE_EXIT(DmStorage::DeleteInstance);
  DmStorage::Instance()->StoreDmToken(kDmToken);
  MockHttpRequest* mock_gone_request = nullptr;
  ASSERT_NO_FATAL_FAILURE(MakeGoneHttpRequest(true, &mock_gone_request));
  // Fetch Policies should fail.
  PolicyResponses responses;
  ASSERT_EQ(
      internal::FetchPolicies(
          DmStorage::Instance(),
          std::move(std::unique_ptr<HttpRequestInterface>(mock_gone_request)),
          CString(kDmToken), kDeviceId, info, &responses),
      HRESULTFromHttpStatusCode(HTTP_STATUS_GONE));
  EXPECT_TRUE(DmStorage::Instance()->GetDmToken().IsEmpty());
}

// Test that we are able to successfully encode and then decode a
// protobuf OmahaSettingsClientProto into a CachedOmahaPolicy instance.
TEST_F(DmClientRequestTest, DecodePolicies) {
  DecodeOmahaPolicies();
}

TEST_F(DmClientRequestTest, HandleDMResponseError) {
  const CPath policy_responses_dir = CPath(ConcatenatePath(
      app_util::GetCurrentModuleDirectory(),
      _T("Policies")));

  std::unique_ptr<DmStorage> dm_storage =
      DmStorage::CreateTestInstance(policy_responses_dir, CString());
  EXPECT_HRESULT_SUCCEEDED(dm_storage->StoreDmToken("dm_token"));

  PolicyResponsesMap responses = {
    {"google/chrome/machine-level-user", "test-data-chr"},
    {"google/earth/machine-level-user",
     "test-data-earth-foo-bar-baz-foo-bar-baz-foo-bar-baz"},
  };

  const PolicyResponses expected_responses = {responses, "expected data"};
  ASSERT_HRESULT_SUCCEEDED(dm_storage->PersistPolicies(expected_responses));

  std::vector<uint8> response;
  EXPECT_TRUE(policy_responses_dir.FileExists());
  EXPECT_TRUE(dm_storage->IsValidDMToken());
  internal::HandleDMResponseError(
      dm_storage.get(), HRESULTFromHttpStatusCode(HTTP_STATUS_GONE), response);
  EXPECT_TRUE(dm_storage->IsInvalidDMToken());
  EXPECT_FALSE(policy_responses_dir.FileExists());

  ASSERT_HRESULT_SUCCEEDED(dm_storage->PersistPolicies(expected_responses));
  enterprise_management::DeviceManagementResponse dm_response;
  dm_response.add_error_detail(
      enterprise_management::CBCM_DELETION_POLICY_PREFERENCE_DELETE_TOKEN);
  std::string response_string;
  ASSERT_TRUE(dm_response.SerializeToString(&response_string));
  response.assign(response_string.begin(), response_string.end());
  internal::HandleDMResponseError(
      dm_storage.get(), HRESULTFromHttpStatusCode(HTTP_STATUS_GONE), response);
  EXPECT_TRUE(dm_storage->GetDmToken().IsEmpty());
  EXPECT_HRESULT_SUCCEEDED(dm_storage->DeleteDmToken());
}

class DmClientRegistryTest : public RegistryProtectedTest {
};

TEST_F(DmClientRegistryTest, GetRegistrationState) {
  // No enrollment token.
  {
    EXPECT_HRESULT_SUCCEEDED(DmStorage::CreateInstance(CString()));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(GetRegistrationState(DmStorage::Instance()), kNotManaged);
  }

  // Enrollment token without device management token.
  {
    EXPECT_HRESULT_SUCCEEDED(
        DmStorage::CreateInstance(_T("57FEBE8F-48D0-487B-A788-CF1019DCD452")));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(GetRegistrationState(DmStorage::Instance()),
              kRegistrationPending);
  }

  // Enrollment token and device management token.
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken("dm_token"));
  {
    EXPECT_HRESULT_SUCCEEDED(
        DmStorage::CreateInstance(_T("57FEBE8F-48D0-487B-A788-CF1019DCD452")));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(GetRegistrationState(DmStorage::Instance()), kRegistered);
  }

  // Device management token without enrollment token.
  {
    EXPECT_HRESULT_SUCCEEDED(DmStorage::CreateInstance(CString()));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(GetRegistrationState(DmStorage::Instance()), kRegistered);
  }
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(""));
}

TEST_F(DmClientRegistryTest, RegisterIfNeeded) {
  // Invalid DM token exists.
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(kInvalidTokenValue));
  {
    EXPECT_HRESULT_SUCCEEDED(
        DmStorage::CreateInstance(_T("57FEBE8F-48D0-487B-A788-CF1019DCD452")));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(RegisterIfNeeded(DmStorage::Instance(), true), E_FAIL);
  }

  // Valid DM token exists.
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken("dm_token"));
  {
    EXPECT_HRESULT_SUCCEEDED(
        DmStorage::CreateInstance(_T("57FEBE8F-48D0-487B-A788-CF1019DCD452")));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(RegisterIfNeeded(DmStorage::Instance(), true), S_FALSE);
  }
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(""));
}

TEST_F(DmClientRegistryTest, RefreshPolicies) {
  // Invalid DM token exists.
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(kInvalidTokenValue));
  {
    EXPECT_HRESULT_SUCCEEDED(
        DmStorage::CreateInstance(_T("57FEBE8F-48D0-487B-A788-CF1019DCD452")));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(RefreshPolicies(), E_FAIL);
  }

  // No DM token.
  ASSERT_NO_FATAL_FAILURE(WriteCompanyDmToken(""));
  {
    EXPECT_HRESULT_SUCCEEDED(
        DmStorage::CreateInstance(_T("57FEBE8F-48D0-487B-A788-CF1019DCD452")));
    ON_SCOPE_EXIT(DmStorage::DeleteInstance);
    EXPECT_EQ(RefreshPolicies(), S_FALSE);
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
  std::vector<std::pair<CString, CString>> params = {
    {_T("one"), _T("1")},
    {_T("2"), _T("two")},
  };

  CString url(kUrl);
  EXPECT_HRESULT_SUCCEEDED(internal::AppendQueryParamsToUrl(params, &url));
  EXPECT_EQ(url, CString(kUrl) + _T("?one=1&2=two"));
}

TEST(DmClientTest, FormatEnrollmentTokenAuthorizationHeader) {
  static const TCHAR kToken[] = _T("token");
  EXPECT_EQ(internal::FormatEnrollmentTokenAuthorizationHeader(kToken),
            _T("GoogleEnrollmentToken token=token"));
}

TEST(DmClientTest, FormatDMTokenAuthorizationHeader) {
  static const TCHAR kToken[] = _T("token");
  EXPECT_EQ(internal::FormatDMTokenAuthorizationHeader(kToken),
            _T("GoogleDMToken token=token"));
}

}  // namespace dm_client
}  // namespace omaha
