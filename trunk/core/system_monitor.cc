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

#include "omaha/core/system_monitor.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"

namespace omaha {

SystemMonitor::SystemMonitor(bool is_machine)
    : is_machine_(is_machine) {
  ::InterlockedExchangePointer(reinterpret_cast<void**>(&observer_), NULL);
}

SystemMonitor::~SystemMonitor() {
  if (m_hWnd) {
    VERIFY1(DestroyWindow());
  }
}

HRESULT SystemMonitor::Initialize(bool monitor_registry) {
  if (monitor_registry) {
    registry_monitor_.reset(new RegistryMonitor);
    if (SUCCEEDED(registry_monitor_->Initialize())) {
      HKEY root_key = is_machine_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
      VERIFY1(SUCCEEDED(registry_monitor_->MonitorValue(
          root_key,
          GOOPDATE_MAIN_KEY,
          kRegValueLastChecked,
          REG_DWORD,
          RegistryValueChangeCallback,
          this)));
      VERIFY1(SUCCEEDED(registry_monitor_->MonitorKey(
          root_key,
          GOOPDATE_REG_RELATIVE_CLIENTS,
          RegistryKeyChangeCallback,
          this)));
      VERIFY1(SUCCEEDED(registry_monitor_->StartMonitoring()));
    }
  }

  // Create a window to receive broadcast messages.
  const TCHAR kWindowTitle[] = _T("{2D905E07-FC38-4b89-83E1-931D3630937F}");
  VERIFY1(Create(NULL, NULL, kWindowTitle));
  return S_OK;
}

LRESULT SystemMonitor::OnPowerBroadcast(UINT, WPARAM wparam,
                                        LPARAM, BOOL& handled) {
  CORE_LOG(L3, (_T("[SystemMonitor::OnPowerBroadcast][wparam %d]"), wparam));
  UNREFERENCED_PARAMETER(wparam);
  handled = true;
  return 0;
}

LRESULT SystemMonitor::OnEndSession(UINT, WPARAM wparam,
                                    LPARAM lparam, BOOL& handled) {
  CORE_LOG(L3, (_T("[SystemMonitor::OnEndSession][wparam %d][lparam %x]"),
                wparam, lparam));
  UNREFERENCED_PARAMETER(wparam);
  UNREFERENCED_PARAMETER(lparam);
  handled = true;
  return 0;
}

LRESULT SystemMonitor::OnQueryEndSession(UINT, WPARAM,
                                         LPARAM lparam, BOOL& handled) {
  CORE_LOG(L3, (_T("[SystemMonitor::OnQueryEndSession][lparam %x]"), lparam));
  UNREFERENCED_PARAMETER(lparam);
  handled = true;
  return TRUE;
}

LRESULT SystemMonitor::OnWTSSessionChange(UINT, WPARAM wparam,
                                          LPARAM lparam, BOOL& handled) {
  CORE_LOG(L3, (_T("[SystemMonitor::OnWTSSessionChange][wparam %x][lparam %d]"),
                wparam, lparam));
  UNREFERENCED_PARAMETER(wparam);
  UNREFERENCED_PARAMETER(lparam);
  handled = true;
  return 0;
}

void SystemMonitor::RegistryValueChangeCallback(const TCHAR* key_name,
                                                const TCHAR* value_name,
                                                RegistryChangeType change_type,
                                                const void* new_value_data,
                                                void* user_data) {
  ASSERT1(key_name);
  ASSERT1(value_name);
  ASSERT1(user_data);

  ASSERT1(_tcscmp(value_name, kRegValueLastChecked) == 0);

  UNREFERENCED_PARAMETER(key_name);
  UNREFERENCED_PARAMETER(value_name);
  UNREFERENCED_PARAMETER(new_value_data);

  SystemMonitor* system_monitor = static_cast<SystemMonitor*>(user_data);
  if (change_type == REGISTRY_CHANGE_TYPE_DELETE &&
      system_monitor->observer_) {
    system_monitor->observer_->LastCheckedDeleted();
  }
}

void SystemMonitor::RegistryKeyChangeCallback(const TCHAR* key_name,
                                              void* user_data) {
  ASSERT1(key_name);
  ASSERT1(user_data);

  UNREFERENCED_PARAMETER(key_name);
  ASSERT1(_tcscmp(key_name, GOOPDATE_REG_RELATIVE_CLIENTS) == 0);

  SystemMonitor* system_monitor = static_cast<SystemMonitor*>(user_data);
  if (!system_monitor->observer_) {
    return;
  }

  const bool is_machine = system_monitor->is_machine_;
  size_t num_clients(0);
  if (SUCCEEDED(goopdate_utils::GetNumClients(is_machine, &num_clients)) &&
      num_clients <= 1) {
    system_monitor->observer_->NoRegisteredClients();
  }
}

}  // namespace omaha
