// Copyright 2011 Google Inc.
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

#include "omaha/goopdate/download_complete_ping_event.h"
#include "omaha/base/string.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/xml_const.h"

namespace omaha {

DownloadCompletePingEvent::DownloadCompletePingEvent(
    Types type,
    Results result,
    int error_code,
    int extra_code1,
    int download_time_ms,
    uint64 num_bytes_downloaded,
    uint64 app_size)
    : PingEvent(type, result, error_code, extra_code1),
      download_time_ms_(download_time_ms),
      num_bytes_downloaded_(num_bytes_downloaded),
      app_size_(app_size) {
}

HRESULT DownloadCompletePingEvent::ToXml(IXMLDOMNode* parent_node) const {
  HRESULT hr = PingEvent::ToXml(parent_node);
  if (FAILED(hr)) {
    return hr;
  }

  // No need to report download metrics if nothing is downloaded.
  if (num_bytes_downloaded_ == 0) {
    return S_OK;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kDownloadTime,
                           itostr(download_time_ms_));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kAppBytesDownloaded,
                           String_Uint64ToString(num_bytes_downloaded_, 10));
  if (FAILED(hr)) {
    return hr;
  }

  return AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kAppBytesTotal,
                             String_Uint64ToString(app_size_, 10));
}

CString DownloadCompletePingEvent::ToString() const {
  CString ping_str;
  ping_str.Format(_T("%s, %s=%s, %s=%s, %s=%s"),
      PingEvent::ToString(),
      xml::attribute::kDownloadTime, itostr(download_time_ms_),
      xml::attribute::kAppBytesDownloaded,
      String_Uint64ToString(num_bytes_downloaded_, 10),
      xml::attribute::kAppBytesTotal, String_Uint64ToString(app_size_, 10));

  return ping_str;
}

}  // namespace omaha

