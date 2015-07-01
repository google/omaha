// Copyright 2014 Google Inc.
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

#include "omaha/common/ping_event_download_metrics.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/xml_const.h"

namespace omaha {

namespace {

// Returns a string literal corresponding to the value of the downloader |d|.
const TCHAR* DownloaderToString(DownloadMetrics::Downloader d) {
  switch (d) {
    case DownloadMetrics::kWinHttp:
      return _T("winhttp");
    case DownloadMetrics::kBits:
      return _T("bits");
    default:
      return _T("unknown");
  }
}

}  // namespace

CString DownloadMetricsToString(const DownloadMetrics& download_metrics) {
  CString result;
  SafeCStringFormat(
      &result,
      _T("url=%s, downloader=%s, error=0x%x, ")
      _T("downloaded_bytes=%I64i, total_bytes=%I64i, download_time=%I64i"),
      download_metrics.url,
      DownloaderToString(download_metrics.downloader),
      download_metrics.error,
      download_metrics.downloaded_bytes,
      download_metrics.total_bytes,
      download_metrics.download_time_ms);
  return result;
}

DownloadMetrics::DownloadMetrics()
    : downloader(kNone),
      error(0),
      downloaded_bytes(0),
      total_bytes(0),
      download_time_ms(0) {
}

PingEventDownloadMetrics::PingEventDownloadMetrics(
    bool is_update,
    Results result,
    const DownloadMetrics& download_metrics)
    : PingEvent(is_update ?
                    EVENT_UPDATE_DOWNLOAD_FINISH :
                    EVENT_INSTALL_DOWNLOAD_FINISH,
                result,
                download_metrics.error,
                0),
      download_metrics_(download_metrics) {
}

HRESULT PingEventDownloadMetrics::ToXml(IXMLDOMNode* parent_node) const {
  HRESULT hr = PingEvent::ToXml(parent_node);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kDownloader,
                           DownloaderToString(download_metrics_.downloader));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kUrl,
                           download_metrics_.url);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(
      parent_node,
      xml::kXmlNamespace,
      xml::attribute::kDownloaded,
      String_Int64ToString(download_metrics_.downloaded_bytes, 10));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(
      parent_node,
      xml::kXmlNamespace,
      xml::attribute::kTotal,
      String_Int64ToString(download_metrics_.total_bytes, 10));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(
      parent_node,
      xml::kXmlNamespace,
      xml::attribute::kDownloadTime,
      String_Int64ToString(download_metrics_.download_time_ms, 10));
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

CString PingEventDownloadMetrics::ToString() const {
  CString ping_str;
  SafeCStringFormat(&ping_str,
                    _T("%s, %s"),
                    PingEvent::ToString(),
                    omaha::DownloadMetricsToString(download_metrics_));

  return ping_str;
}

}  // namespace omaha
