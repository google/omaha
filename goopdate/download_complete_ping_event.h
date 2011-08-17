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

#ifndef OMAHA_GOOPDATE_DOWNLOAD_COMPLETE_PING_EVENT_H_
#define OMAHA_GOOPDATE_DOWNLOAD_COMPLETE_PING_EVENT_H_

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/ping_event.h"

namespace omaha {

class DownloadCompletePingEvent : public PingEvent {
 public:
  DownloadCompletePingEvent(Types type,
                            Results result,
                            int error_code,
                            int extra_code1,
                            int download_time_ms,
                            uint64 num_bytes_downloaded,
                            uint64 app_size);
  virtual ~DownloadCompletePingEvent() {}

  virtual HRESULT ToXml(IXMLDOMNode* parent_node) const;
  virtual CString ToString() const;

 private:
  const int download_time_ms_;
  const uint64 num_bytes_downloaded_;
  const uint64 app_size_;
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DOWNLOAD_COMPLETE_PING_EVENT_H_
