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
// Test app for the Google Update recovery mechanism.
#include <windows.h>
#include <atlstr.h>
#include "omaha/common/const_addresses.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/google_update_recovery.h"
#include "omaha/common/logging.h"
#include "omaha/net/network_config.h"
#include "omaha/net/network_request.h"

using omaha::GoogleProxyDetector;
using omaha::FirefoxProxyDetector;
using omaha::IEProxyDetector;
using omaha::NetworkRequest;
using omaha::NetworkConfig;

namespace {

// Implements the DownloadFileMethod signature.
HRESULT DownloadFile(const TCHAR* url, const TCHAR* file_path, void*) {
  UTIL_LOG(L2, (_T("[DownloadFile][%s][%s]"), url, file_path));

  ASSERT1(url);
  ASSERT1(file_path);

  CString test_url(url);
  CString test_file_path(file_path);

// Include this code to override the address portion of the URL.
#if 1
  // Change the URL below to point to a test repair exe of your own.
  const TCHAR* const kTestAddress =
      _T("http://dl.google.com/insert_your_file_here?");

  VERIFY1(1 == test_url.Replace(omaha::kUrlCodeRedCheck, kTestAddress));
#endif

  // Initialize the network for user with no impersonation required.
  NetworkConfig& network_config = NetworkConfig::Instance();
  network_config.Initialize(false, NULL);
  network_config.Add(new GoogleProxyDetector(MACHINE_REG_UPDATE_DEV));
  network_config.Add(new FirefoxProxyDetector());
  network_config.Add(new IEProxyDetector());

  NetworkRequest network_request(network_config.session());
  HRESULT hr = network_request.DownloadFile(test_url, CString(file_path));

  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[DownloadFile failed][%s][0x%08x]"), test_url, hr));
    return hr;
  }

  int status_code = network_request.http_status_code();
  UTIL_LOG(L2, (_T("[HTTP status][%u]"), status_code));

  return (HTTP_STATUS_OK == status_code) ? S_OK : E_FAIL;
}

}  // namespace

int main() {
  const TCHAR* const kDummyGuid = _T("{20F8FA32-8E16-4046-834B-88661E021AFC}");

  HRESULT hr = FixGoogleUpdate(kDummyGuid,
                               _T("1.0.555.0"),
                               _T("en-us"),
                               true,
                               DownloadFile,
                               NULL);

  return hr;
}
