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

// Ping requests use http protocol. When pinging over http fails, a fallback
// using https protocol is attempted.

#include "omaha/worker/ping.h"

#include <atlstr.h>
#include <vector>
#include "omaha/common/string.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/goopdate_xml_parser.h"
#include "omaha/goopdate/request.h"
#include "omaha/net/browser_request.h"
#include "omaha/net/network_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

Ping::Ping() {
  // The ping request does not use CUP since its response does not require
  // authentication.
  const NetworkConfig::Session& session(NetworkConfig::Instance().session());
  network_request_.reset(new NetworkRequest(session));
  network_request_->AddHttpRequest(new SimpleRequest);
  network_request_->AddHttpRequest(new BrowserRequest);
}

Ping::~Ping() {
}

// Returns S_OK without sending the ping in OEM mode.
HRESULT Ping::SendPing(Request* req) {
  CORE_LOG(L2, (_T("[Ping::SendPing]")));
  ASSERT1(req);

  // Do not access the network during an OEM install.
  if (!ConfigManager::Instance()->CanUseNetwork(req->is_machine())) {
    CORE_LOG(L1, (_T("[Ping not sent because network use prohibited]")));
    return GOOPDATE_E_CANNOT_USE_NETWORK;
  }

  // Do not send a request which contains no ping events.
  bool has_ping_events = false;
  for (AppRequestVector::const_iterator it = req->app_requests_begin();
       it != req->app_requests_end();
       ++it) {
    if ((*it).request_data().num_ping_events() != 0) {
      has_ping_events = true;
      break;
    }
  }
  if (!has_ping_events) {
    return HRESULT_FROM_WIN32(ERROR_NO_DATA);
  }

  HighresTimer metrics_timer;

  CString request_string;
  HRESULT hr = GoopdateXmlParser::GenerateRequest(*req,
                                                  false,
                                                  &request_string);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GenerateRequest failed][0x%08x]"), hr));
    return hr;
  }

  CString ping_url;
  hr = ConfigManager::Instance()->GetPingUrl(&ping_url);
  if (FAILED(hr)) {
    return hr;
  }

  hr = DoSendPing(ping_url, request_string);
  if (FAILED(hr)) {
    metric_ping_failed_ms.AddSample(metrics_timer.GetElapsedMs());
    return hr;
  }

  metric_ping_succeeded_ms.AddSample(metrics_timer.GetElapsedMs());
  return S_OK;
}

HRESULT Ping::DoSendPing(const CString& url,
                         const CString& request_string) {
  CString response;
  return PostRequest(network_request_.get(),
                     true,                    // Fall back to https.
                     url,
                     request_string,
                     &response);
}

HRESULT Ping::Cancel() {
  if (network_request_.get()) {
    HRESULT hr = network_request_->Cancel();
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[NetworkRequest::Cancel failed][0x%08x]"), hr));
      return hr;
    }
  }

  return S_OK;
}

}  // namespace omaha
