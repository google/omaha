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

#ifndef OMAHA_CLIENT_SHUTDOWN_EVENTS_H_
#define OMAHA_CLIENT_SHUTDOWN_EVENTS_H_

#include <windows.h>
#include <atlsafe.h>
#include "base/scoped_ptr.h"
#include "omaha/base/shutdown_callback.h"
#include "omaha/base/synchronized.h"

namespace omaha {

class BundleInstaller;
class Reactor;
class ShutdownHandler;

class ShutdownEvents : public ShutdownCallback {
 public:
  explicit ShutdownEvents(BundleInstaller* installer);
  virtual ~ShutdownEvents();

  HRESULT InitializeShutdownHandler(bool is_machine);
  HRESULT Shutdown();
  static HRESULT CreateShutdownHandler(bool is_machine,
                                       BundleInstaller* installer,
                                       ShutdownCallback** shutdown_callback);

 private:
  BundleInstaller* installer_;

  LLock lock_;
  scoped_ptr<Reactor> reactor_;
  scoped_ptr<ShutdownHandler> shutdown_handler_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ShutdownEvents);
};

}  // namespace omaha

#endif  // OMAHA_CLIENT_SHUTDOWN_EVENTS_H_
