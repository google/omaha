// Copyright 2006-2009 Google Inc.
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
// A simple interface for monitoring changes
// happening to a store.
//
#ifndef OMAHA_COMMON_STORE_WATCHER_H_
#define OMAHA_COMMON_STORE_WATCHER_H_

#include "omaha/common/synchronized.h"

namespace omaha {

// Allows for monitoring changes happening to a store
// (independant of what the underlying store is).
class StoreWatcher {
 public:
  StoreWatcher() {}
  virtual ~StoreWatcher() {}

  // Called to create/reset the event that gets signaled
  // any time the store changes.  Access the created
  // event using change_event().
  virtual HRESULT EnsureEventSetup() = 0;

  // Indicates if any changes have occured
  bool HasChangeOccurred() const {
    return IsHandleSignaled(change_event());
  }

  // Get the event that is signaled on store changes.
  // Note:
  //   * This event will remain constant until the class is destroyed.
  //   * One should call EnsureEventSetup to set-up the event.
  //   * The event is only signaled on the next change and remains signaled.
  //     Do not call ::ResetEvent(). Call EnsureEventSetup() to reset
  //     the event and wait for more changes.
  virtual HANDLE change_event() const = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(StoreWatcher);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_STORE_WATCHER_H_
