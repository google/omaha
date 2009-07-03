// Copyright 2009 Google Inc.
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

// SystemMonitor receives session messages and power management messages.

#ifndef OMAHA_CORE_SYSTEM_MONITOR_H_
#define OMAHA_CORE_SYSTEM_MONITOR_H_

#include <atlbase.h>
#include <atlwin.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/registry_monitor_manager.h"

namespace omaha {

class SystemMonitorObserver {
 public:
  virtual ~SystemMonitorObserver() {}

  // Called when 'LastChecked' registry value is deleted.
  virtual void LastCheckedDeleted() = 0;

  // Called when there are no other clients that are registered besides Omaha.
  virtual void NoRegisteredClients() = 0;
};

class SystemMonitor
    : public CWindowImpl<SystemMonitor,
                         CWindow,
                         CWinTraits<WS_OVERLAPPED, WS_EX_TOOLWINDOW> > {
 public:
  explicit SystemMonitor(bool is_machine);
  ~SystemMonitor();

  HRESULT Initialize(bool monitor_registry);

  void set_observer(SystemMonitorObserver* observer) {
    ::InterlockedExchangePointer(reinterpret_cast<void**>(&observer_),
                                 observer);
  }

  BEGIN_MSG_MAP(SystemMonitor)
    MESSAGE_HANDLER(WM_POWERBROADCAST,    OnPowerBroadcast)
    MESSAGE_HANDLER(WM_QUERYENDSESSION,   OnQueryEndSession)
    MESSAGE_HANDLER(WM_ENDSESSION,        OnEndSession)
    MESSAGE_HANDLER(WM_WTSSESSION_CHANGE, OnWTSSessionChange)
  END_MSG_MAP()

 private:
  // Notifies the system monitor that a power-management event has occurred.
  LRESULT OnPowerBroadcast(UINT msg, WPARAM wparam,
                           LPARAM lparam, BOOL& handled);

  // Notifies the system monitor a shutdown has been requested.
  LRESULT OnQueryEndSession(UINT msg, WPARAM wparam,
                            LPARAM lparam, BOOL& handled);

  // Notifies the system monitor whether the session is ending.
  LRESULT OnEndSession(UINT msg, WPARAM wparam,
                       LPARAM lparam, BOOL& handled);

  // Notifies the system monitor about changes in the session state.
  LRESULT OnWTSSessionChange(UINT msg, WPARAM wparam,
                             LPARAM lparam, BOOL& handled);

  static void RegistryValueChangeCallback(const TCHAR* key_name,
                                          const TCHAR* value_name,
                                          RegistryChangeType change_type,
                                          const void* new_value_data,
                                          void* user_data);

  static void RegistryKeyChangeCallback(const TCHAR* key_name,
                                        void* user_data);

  scoped_ptr<RegistryMonitor> registry_monitor_;
  bool is_machine_;
  SystemMonitorObserver* observer_;

  DISALLOW_EVIL_CONSTRUCTORS(SystemMonitor);
};

}  // namespace omaha

#endif  // OMAHA_CORE_SYSTEM_MONITOR_H_

