// Copyright 2008-2009 Google Inc.
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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_CONSOLE_WRITER_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_CONSOLE_WRITER_H_

#include <Windows.h>
#include <tchar.h>
#include "omaha/common/synchronized.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/worker/ping_event.h"
#include "omaha/tools/omahacompatibility/common/ping_observer.h"

namespace omaha {

// Observer that writes to the console. It also remembers
// the result for install and update completed event.
class ConsoleWriter : public PingObserver {
 public:
  explicit ConsoleWriter(const CString& app_guid)
      : app_guid_(app_guid),
        install_completed_(false),
        install_result_(PingEvent::EVENT_RESULT_ERROR),
        update_completed_(false),
        update_result_(PingEvent::EVENT_RESULT_ERROR) {}
  virtual ~ConsoleWriter() {}
  virtual void Observe(const AppRequestData& data);

  // Indicates if a install complete event has been received.
  bool install_completed() const {
    return install_completed_;
  }
  PingEvent::Results install_result() const {
    return install_result_;
  }

  // Indicates if a update complete event has been received.
  bool update_completed() const {
    return update_completed_;
  }
  PingEvent::Results update_result() const {
    return update_result_;
  }

 private:
  bool IsInstallCompletedEvent(const CString& app_guid,
                               const PingEvent& ping);
  bool IsUpdateCompletedEvent(const CString& app_guid,
                              const PingEvent& ping);
  CString PingResultToString(PingEvent::Results result);
  CString PingTypeToString(PingEvent::Types type);

  CString app_guid_;
  LLock lock_;
  PingEvent::Results install_result_;
  bool install_completed_;
  PingEvent::Results update_result_;
  bool update_completed_;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_CONSOLE_WRITER_H_
