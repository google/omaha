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

#ifndef OMAHA_GOOPDATE_PING_EVENT_CANCEL_H_
#define OMAHA_GOOPDATE_PING_EVENT_CANCEL_H_

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/ping_event.h"

namespace omaha {

class PingEventCancel : public PingEvent {
 public:
  PingEventCancel(Types type,
                  Results result,
                  int error_code,
                  int extra_code1,
                  bool is_bundled,
                  int state_when_cancelled,
                  int time_since_update_available_ms,
                  int time_since_download_start_ms);
  virtual ~PingEventCancel() {}

  virtual HRESULT ToXml(IXMLDOMNode* parent_node) const;
  virtual CString ToString() const;

 private:
  const bool is_bundled_;
  const int state_when_cancelled_;
  const int time_since_update_available_ms_;
  const int time_since_download_start_ms_;
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_PING_EVENT_CANCEL_H_
