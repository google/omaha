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

//
// ShutdownHandler monitors a shutdown event.

#ifndef OMAHA_COMMON_SHUTDOWN_HANDLER_H__
#define OMAHA_COMMON_SHUTDOWN_HANDLER_H__

#include <windows.h>
#include "base/basictypes.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/event_handler.h"

namespace omaha {

class Reactor;
class ShutdownCallback;

class ShutdownHandler : public EventHandler {
 public:
  ShutdownHandler();
  ~ShutdownHandler();

  HRESULT Initialize(Reactor* reactor,
                     ShutdownCallback* shutdown,
                     bool is_machine);
  virtual void HandleEvent(HANDLE handle);

 private:
  Reactor* reactor_;
  scoped_event shutdown_event_;
  ShutdownCallback* shutdown_callback_;
  bool is_machine_;
  DISALLOW_EVIL_CONSTRUCTORS(ShutdownHandler);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SHUTDOWN_HANDLER_H__

