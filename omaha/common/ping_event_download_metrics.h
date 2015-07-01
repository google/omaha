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

#ifndef OMAHA_COMMON_PING_EVENT_DOWNLOAD_METRICS_H_
#define OMAHA_COMMON_PING_EVENT_DOWNLOAD_METRICS_H_

#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/ping_event.h"

namespace omaha {

struct DownloadMetrics {
  enum Downloader { kNone = 0, kWinHttp, kBits };

  DownloadMetrics();

  CString url;

  Downloader downloader;

  // Contains 0 if the download was successful, an HTTP status code encoded
  // as an HRESULT, or an actual HRESULT.
  int error;

  int64 downloaded_bytes;  // -1 means that the byte count is unknown.
  int64 total_bytes;

  int64 download_time_ms;
};

CString DownloadMetricsToString(const DownloadMetrics& download_metrics);

class PingEventDownloadMetrics : public PingEvent {
 public:
  PingEventDownloadMetrics(bool is_update,
                           Results result,
                           const DownloadMetrics& download_metrics);
  virtual ~PingEventDownloadMetrics() {}

  virtual HRESULT ToXml(IXMLDOMNode* parent_node) const;
  virtual CString ToString() const;

 private:
  const DownloadMetrics download_metrics_;

  DISALLOW_COPY_AND_ASSIGN(PingEventDownloadMetrics);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_PING_EVENT_DOWNLOAD_METRICS_H_
