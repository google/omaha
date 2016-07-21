// Copyright 2013 Google Inc.
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

#include "omaha/goopdate/ping_event_cancel.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/xml_const.h"

namespace omaha {

PingEventCancel::PingEventCancel(Types type,
                                 Results result,
                                 int error_code,
                                 int extra_code1,
                                 bool is_bundled,
                                 int state_when_cancelled,
                                 int time_since_update_available_ms,
                                 int time_since_download_start_ms)
    : PingEvent(type, result, error_code, extra_code1),
      is_bundled_(is_bundled),
      state_when_cancelled_(state_when_cancelled),
      time_since_update_available_ms_(time_since_update_available_ms),
      time_since_download_start_ms_(time_since_download_start_ms) {
}

HRESULT PingEventCancel::ToXml(IXMLDOMNode* parent_node) const {
  HRESULT hr = PingEvent::ToXml(parent_node);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kIsBundled,
                           itostr(is_bundled_));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kStateCancelled,
                           itostr(state_when_cancelled_));
  if (FAILED(hr)) {
    return hr;
  }

  if (time_since_update_available_ms_ >= 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kTimeSinceUpdateAvailable,
                             itostr(time_since_update_available_ms_));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (time_since_download_start_ms_ >= 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kTimeSinceDownloadStart,
                             itostr(time_since_download_start_ms_));
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

CString PingEventCancel::ToString() const {
  CString time_since_update_available_str;
  if (time_since_update_available_ms_ >= 0) {
    SafeCStringFormat(&time_since_update_available_str,
                      _T(", %s=%s"),
                      xml::attribute::kTimeSinceUpdateAvailable,
                      itostr(time_since_update_available_ms_));
  }

  CString time_since_download_start_str;
  if (time_since_download_start_ms_ >= 0) {
    SafeCStringFormat(&time_since_download_start_str,
                      _T(", %s=%s"),
                      xml::attribute::kTimeSinceDownloadStart,
                      itostr(time_since_download_start_ms_));
  }

  CString ping_str;
  SafeCStringFormat(&ping_str, _T("%s, %s=%s, %s=%s%s%s"),
                    PingEvent::ToString(),
                    xml::attribute::kIsBundled,
                    itostr(is_bundled_),
                    xml::attribute::kStateCancelled,
                    itostr(state_when_cancelled_),
                    time_since_update_available_str,
                    time_since_download_start_str);

  return ping_str;
}

}  // namespace omaha

