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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMMON_PING_OBSERVER_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMMON_PING_OBSERVER_H_

#include "omaha/worker/app_request_data.h"

namespace omaha {

// Interface for receiving ping events from the http server.
class PingObserver {
 public:
  virtual ~PingObserver() {}
  virtual void Observe(const AppRequestData& data) = 0;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMMON_PING_OBSERVER_H_
