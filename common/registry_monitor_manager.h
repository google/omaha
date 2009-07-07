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
//
// The RegistryMonitor allows a caller to request monitoring
// for registry value changes across multiple keys. It uses the Windows API
// function RegNotifyChangeKeyValue to notify when any value in a key changes.

#ifndef OMAHA_COMMON_REGISTRY_MONITOR_MANAGER_H_
#define OMAHA_COMMON_REGISTRY_MONITOR_MANAGER_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace omaha {

namespace detail {

class RegistryMonitorImpl;

}  // namespace detail

// Called when a registry value changes. 'new_value_data' contains a
// pointer to the string data for a string value or the value itself for a
// DWORD value.
enum RegistryChangeType {
  REGISTRY_CHANGE_TYPE_CREATE = 0,
  REGISTRY_CHANGE_TYPE_UPDATE,
  REGISTRY_CHANGE_TYPE_DELETE,
};
typedef void (*RegistryValueChangeCallback)(const TCHAR* key_name,
                                            const TCHAR* value_name,
                                            RegistryChangeType change_type,
                                            const void* new_value_data,
                                            void* user_data);

// Called when a registry key changes. Changes include subkeys being
// created or deleted as well as value changes under that key but not under
// the subkeys of the key.
typedef void (*RegistryKeyChangeCallback)(const TCHAR* key_name,
                                          void* user_data);

class RegistryMonitor {
 public:
  RegistryMonitor();
  ~RegistryMonitor();

  HRESULT Initialize();

  // Monitors a registry sub key for changes. Registering the same sub key
  // overrides the previous registration.
  HRESULT MonitorKey(HKEY root_key,
                     const CString& sub_key,
                     RegistryKeyChangeCallback callback,
                     void* user_data);

  // Adds a registry value to the list of values to monitor for changes.
  // All values must be registered before starting monitoring. Registering
  // the same value is allowed, although not particularly useful.
  HRESULT MonitorValue(HKEY root_key,
                       const CString& sub_key,
                       const CString& value_name,
                       int value_type,
                       RegistryValueChangeCallback callback,
                       void* user_data);

  // Starts monitoring for changes.
  HRESULT StartMonitoring();

 private:
  scoped_ptr<detail::RegistryMonitorImpl> impl_;

  DISALLOW_EVIL_CONSTRUCTORS(RegistryMonitor);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_REGISTRY_MONITOR_MANAGER_H_

