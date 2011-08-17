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
// This is for Omaha2 backwards compatibility.
// The installed Omaha3 handoff process sets an event to tell an Omaha2 setup
// worker running from the temp directory that a UI has been displayed so that
// the Omaha2 worker will not display a second UI on error. The event's name is
// passed in an environment variable name by the Omaha2 worker.

#ifndef OMAHA_UI_UI_DISPLAYED_EVENT_H_
#define OMAHA_UI_UI_DISPLAYED_EVENT_H_

#include <windows.h>
#include "omaha/base/scoped_any.h"

namespace omaha {

// Manages the UI Displayed Event, which is used to communicate whether a UI
// has been displayed between processes.
// This class is not thread safe.
class UIDisplayedEventManager {
 public:
  // Signals the event. Creates it if its name does not already exist in the
  // environment variable.
  static void SignalEvent(bool is_machine);

 private:
   // Creates the event and sets its name in the environment variable.
   static HRESULT CreateEvent(bool is_machine);

   // Gets the event from the name in the environment variable.
   static HRESULT GetEvent(bool is_machine, HANDLE* ui_displayed_event);

  // Returns whether this process's event handle has been initialized.
  static bool IsEventHandleInitialized();

  // A single instance of the UI Displayed Event handle to be used for the
  // lifetime of this process.
  static scoped_handle ui_displayed_event_;
};

}  // namespace omaha

#endif  // OMAHA_UI_UI_DISPLAYED_EVENT_H_
