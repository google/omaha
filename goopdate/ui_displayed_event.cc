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

#include "omaha/goopdate/ui_displayed_event.h"
#include "omaha/common/const_object_names.h"
#include "omaha/goopdate/goopdate_utils.h"

namespace omaha {

HRESULT UIDisplayedEventManager::CreateEvent(bool is_machine) {
  ASSERT1(!IsEventHandleInitialized());
  return goopdate_utils::CreateUniqueEventInEnvironment(
        kUiDisplayedEventEnvironmentVariableName,
        is_machine,
        address(ui_displayed_event_));
}

// Caller does not own the event handle and must not close it.
// There is a single event handle for each process. The handle is closed when
// the process exits.
HRESULT UIDisplayedEventManager::GetEvent(bool is_machine,
                                          HANDLE* ui_displayed_event) {
  ASSERT1(ui_displayed_event);
  *ui_displayed_event = NULL;
  if (IsEventHandleInitialized()) {
    *ui_displayed_event = get(ui_displayed_event_);
    return S_OK;
  }

  HRESULT hr = goopdate_utils::OpenUniqueEventFromEnvironment(
      kUiDisplayedEventEnvironmentVariableName,
      is_machine,
      address(ui_displayed_event_));
  if (FAILED(hr)) {
    return hr;
  }

  *ui_displayed_event = get(ui_displayed_event_);
  return S_OK;
}

// Creates the event if it does not already exist in the environment.
void UIDisplayedEventManager::SignalEvent(bool is_machine) {
  CORE_LOG(L2, (_T("[SignalEvent]")));

  if (!IsEventHandleInitialized()) {
    HRESULT hr = GetEvent(is_machine, address(ui_displayed_event_));
    if (HRESULT_FROM_WIN32(ERROR_ENVVAR_NOT_FOUND) == hr) {
      // The event was not created by an earlier process. This can happen when
      // developers run the /handoff process directly.
      hr = CreateEvent(is_machine);
    }
    if (FAILED(hr)) {
      reset(ui_displayed_event_);
      // We may display two UIs.
      return;
    }
  }

  ASSERT1(IsEventHandleInitialized());
  VERIFY1(::SetEvent(get(ui_displayed_event_)));
}

// Returns false if the event does not exist in the environment variable.
bool UIDisplayedEventManager::HasUIBeenDisplayed(bool is_machine) {
  HANDLE ui_displayed_event(NULL);
  HRESULT hr = GetEvent(is_machine, &ui_displayed_event);
  if (FAILED(hr)) {
    return false;
  }

  int res = ::WaitForSingleObject(ui_displayed_event, 0);
  ASSERT1(WAIT_OBJECT_0 == res || WAIT_TIMEOUT == res);
  return WAIT_OBJECT_0 == res;
}

bool UIDisplayedEventManager::IsEventHandleInitialized() {
  return valid(ui_displayed_event_);
}

scoped_handle UIDisplayedEventManager::ui_displayed_event_;

}  // namespace omaha
