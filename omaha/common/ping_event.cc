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

#include "omaha/common/ping_event.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/xml_const.h"

namespace omaha {

PingEvent::PingEvent(Types type,
                     Results result,
                     int error_code,
                     int extra_code1)
    : event_type_(type),
      event_result_(result),
      error_code_(error_code),
      extra_code1_(extra_code1),
      source_url_index_(-1),
      update_check_time_ms_(0),
      download_time_ms_(0),
      num_bytes_downloaded_(0),
      app_size_(0),
      install_time_ms_(0) {
  ASSERT1(EVENT_UNKNOWN != event_type_);
}

PingEvent::PingEvent(Types type,
                     Results result,
                     int error_code,
                     int extra_code1,
                     int source_url_index,
                     int update_check_time_ms,
                     int download_time_ms,
                     uint64 num_bytes_downloaded,
                     uint64 app_size,
                     int install_time_ms)
    : event_type_(type),
      event_result_(result),
      error_code_(error_code),
      extra_code1_(extra_code1),
      source_url_index_(source_url_index),
      update_check_time_ms_(update_check_time_ms),
      download_time_ms_(download_time_ms),
      num_bytes_downloaded_(num_bytes_downloaded),
      app_size_(app_size),
      install_time_ms_(install_time_ms) {
  ASSERT1(EVENT_UNKNOWN != event_type_);
}

HRESULT PingEvent::ToXml(IXMLDOMNode* parent_node) const {
  HRESULT hr = AddXMLAttributeNode(parent_node,
                                   xml::kXmlNamespace,
                                   xml::attribute::kEventType,
                                   itostr(event_type_));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kEventResult,
                           itostr(event_result_));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kErrorCode,
                           itostr(error_code_));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kExtraCode1,
                           itostr(extra_code1_));
  if (FAILED(hr)) {
    return hr;
  }

  if (source_url_index_ >= 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kSourceUrlIndex,
                             itostr(source_url_index_));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (update_check_time_ms_ != 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kUpdateCheckTime,
                             itostr(update_check_time_ms_));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (download_time_ms_ != 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kDownloadTime,
                             itostr(download_time_ms_));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (num_bytes_downloaded_ != 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kAppBytesDownloaded,
                             String_Uint64ToString(num_bytes_downloaded_, 10));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (app_size_ != 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kAppBytesTotal,
                             String_Uint64ToString(app_size_, 10));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (install_time_ms_ != 0) {
    hr = AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kInstallTime,
                             itostr(install_time_ms_));
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

CString PingEvent::ToString() const {
  CString ping_str;
  SafeCStringFormat(&ping_str, _T("%s=%s, %s=%s, %s=%s, %s=%s"),
                    xml::attribute::kEventType, itostr(event_type_),
                    xml::attribute::kEventResult, itostr(event_result_),
                    xml::attribute::kErrorCode, itostr(error_code_),
                    xml::attribute::kExtraCode1, itostr(extra_code1_));

  if (source_url_index_ >= 0) {
    SafeCStringAppendFormat(&ping_str, _T(", %s=%s"),
                            xml::attribute::kSourceUrlIndex,
                            itostr(source_url_index_));
  }

  if (update_check_time_ms_ != 0) {
    SafeCStringAppendFormat(&ping_str, _T(", %s=%s"),
                            xml::attribute::kUpdateCheckTime,
                            itostr(update_check_time_ms_));
  }

  if (app_size_ != 0) {
    SafeCStringAppendFormat(&ping_str, _T(", %s=%s, %s=%s, %s=%s"),
                            xml::attribute::kDownloadTime,
                            itostr(download_time_ms_),
                            xml::attribute::kAppBytesDownloaded,
                            String_Uint64ToString(num_bytes_downloaded_, 10),
                            xml::attribute::kAppBytesTotal,
                            String_Uint64ToString(app_size_, 10));
  }

  if (install_time_ms_ != 0) {
    SafeCStringAppendFormat(&ping_str, _T(", %s=%s"),
                            xml::attribute::kInstallTime,
                            itostr(install_time_ms_));
  }

  return ping_str;
}

}  // namespace omaha
