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
// RegistryMonitor creates a KeyWatcher for every unique registry key that
// contains a value registered by MonitorValue. Each KeyWatcher is responsible
// for monitoring one or more values in a single registry key but not its
// subkeys. RegistryMonitor manages a thread which waits on event objects.
// The events are signaled when the corresponding monitored key changes.

#include "omaha/common/registry_monitor_manager.h"
#include <atlbase.h>
#include <utility>
#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/thread.h"

namespace omaha {

namespace detail {

// converts a registry change type value to a string for logging purposes.
CString RegistryChangeTypeToString(RegistryChangeType registry_change_type) {
  switch (registry_change_type) {
  case REGISTRY_CHANGE_TYPE_CREATE:
    return _T("create");
  case REGISTRY_CHANGE_TYPE_UPDATE:
    return _T("update");
  case REGISTRY_CHANGE_TYPE_DELETE:
    return _T("delete");
  default:
    ASSERT1(false);
    return _T("unknown");
  }
};

// Holds a pair of root key and sub key, as monitoring must be unique for
// each pair.
class KeyId {
 public:
  KeyId(HKEY parent_key, const CString& key_name)
      : parent_key_(parent_key), key_name_(key_name) {}

  HKEY parent_key() const { return parent_key_; }
  CString key_name() const { return key_name_; }

  static bool IsEqual(const KeyId& id1, const KeyId& id2) {
    return id1.parent_key_ == id2.parent_key_ &&
           id1.key_name_   == id2.key_name_;
  }
 private:
  HKEY    parent_key_;
  CString key_name_;
};

class KeyWatcher;

// ValueWatcher represents a single monitored registry value.
// It is used by KeyWatcher to determine which registry value has changed when
// it detects a change in its key.
class ValueWatcher {
 public:
  ValueWatcher(KeyWatcher* key_watcher,
               const CString& value_name,
               int value_type,
               RegistryValueChangeCallback callback,
               void* user_data);
  ~ValueWatcher();

  // Returns true if the initial value has changed.
  bool HasChanged();

  // Calls the callback function to do the notification of the change.
  void DoCallback();

 private:
  CString GetCurrentValueString();
  DWORD   GetCurrentValueDword();

  CString last_known_value_string_;
  DWORD last_known_value_dword_;
  bool value_is_valid_;
  RegistryChangeType change_type_;
  CString value_name_;
  int value_type_;
  KeyWatcher* key_watcher_;
  RegistryValueChangeCallback callback_;
  void* callback_param_;
};


// KeyWatcher is responsible for monitoring changes to a single key in the
// Windows registry. RegistryMonitor keeps a container of KeyWatcher objects,
// one object for each key that contains a value to be monitored.
class KeyWatcher {
 public:
  explicit KeyWatcher(const KeyId& key_id);

  ~KeyWatcher();

  // Adds a new registry value to monitor.
  HRESULT AddValue(const CString& value_name,
                   int value_type,
                   RegistryValueChangeCallback callback,
                   void* user_data);

  // Registers the key watcher with the OS and gets ready to receive events.
  HRESULT StartWatching();

  // Returns true if the underlying registry handle corresponds to a valid key.
  bool IsKeyValid();

  HANDLE notification_event() const { return get(notification_event_); }

  RegKey& key() { return key_; }

  CString key_name() const { return key_id_.key_name(); }

  void set_callback(RegistryKeyChangeCallback callback, void* callback_param) {
    callback_       = callback;
    callback_param_ = callback_param;
  }

  // Callback called when the notification event is signaled by the OS
  // as a result of a change in the monitored key.
  void HandleEvent(HANDLE handle);

 private:
  // Ensures the key to monitor is always open.
  HRESULT EnsureOpen();

  std::vector<ValueWatcher*> values_;
  RegKey key_;
  const KeyId key_id_;
  scoped_event notification_event_;

  RegistryKeyChangeCallback callback_;
  void* callback_param_;

  DISALLOW_EVIL_CONSTRUCTORS(KeyWatcher);
};

class RegistryMonitorImpl : public Runnable {
 public:
  RegistryMonitorImpl();
  ~RegistryMonitorImpl();

  HRESULT MonitorKey(HKEY root_key,
                     const CString& sub_key,
                     RegistryKeyChangeCallback callback,
                     void* user_data);

  HRESULT MonitorValue(HKEY root_key,
                       const CString& sub_key,
                       const CString& value_name,
                       int value_type,
                       RegistryValueChangeCallback callback,
                       void* user_data);

  HRESULT Initialize();

  HRESULT StartMonitoring();

 private:

  // Runnable.
  virtual void Run();

  typedef std::pair<KeyId, KeyWatcher*> Watcher;
  std::vector<Watcher> watchers_;

  Thread thread_;
  scoped_ptr<Gate> start_monitoring_gate_;
  scoped_event stop_monitoring_;
  DISALLOW_EVIL_CONSTRUCTORS(RegistryMonitorImpl);
};


ValueWatcher::ValueWatcher(KeyWatcher* key_watcher,
                           const CString &value_name,
                           int value_type,
                           RegistryValueChangeCallback callback,
                           void* user_data)
    : key_watcher_(key_watcher),
      value_is_valid_(false),
      change_type_(REGISTRY_CHANGE_TYPE_CREATE),
      callback_(callback),
      callback_param_(user_data),
      value_name_(value_name),
      value_type_(value_type),
      last_known_value_dword_(0) {
  ASSERT1(key_watcher);
  ASSERT1(callback);
  if (value_type_ == REG_SZ) {
    last_known_value_string_ = GetCurrentValueString();
  } else if (value_type_ == REG_DWORD) {
    last_known_value_dword_ = GetCurrentValueDword();
  } else {
    ASSERT(false, (_T("value type not supported")));
  }
}

ValueWatcher::~ValueWatcher() {
}

bool ValueWatcher::HasChanged() {
  UTIL_LOG(L3, (_T("[ValueWatcher::HasChanged]")
                _T("[key name '%s'][value '%s'][valid %d]"),
                key_watcher_->key_name(), value_name_, value_is_valid_));

  const bool value_was_valid = value_is_valid_;

  bool has_changed = false;
  if (value_type_ == REG_SZ) {
    CString new_value = GetCurrentValueString();
    has_changed = last_known_value_string_ != new_value;

    UTIL_LOG(L3, (_T("[ValueWatcher::HasChanged][old value %s][new value %s]"),
                  last_known_value_string_, new_value));

    last_known_value_string_ = new_value;
  } else if (value_type_ == REG_DWORD) {
    DWORD new_value = GetCurrentValueDword();
    has_changed = last_known_value_dword_ != new_value;

    UTIL_LOG(L3, (_T("[ValueWatcher::HasChanged][old value %d][new value %d]"),
                  last_known_value_dword_, new_value));

    last_known_value_dword_ = new_value;
  } else {
    ASSERT(false, (_T("value type not supported")));
  }

  // Detect the type of the change based on previous and current value state.
  if (value_was_valid && value_is_valid_) {
    change_type_ = REGISTRY_CHANGE_TYPE_UPDATE;
  } else if (value_was_valid && !value_is_valid_) {
    change_type_ = REGISTRY_CHANGE_TYPE_DELETE;
  } else if (!value_was_valid && value_is_valid_) {
    change_type_ = REGISTRY_CHANGE_TYPE_CREATE;
  } else {
    ASSERT1(!value_was_valid && !value_is_valid_);
  }

  if (has_changed) {
    UTIL_LOG(L3, (_T("[ValueWatcher::HasChanged]")
                  _T("[key name '%s'][value '%s' has changed][%s]"),
                  key_watcher_->key_name(), value_name_,
                  RegistryChangeTypeToString(change_type_)));
  } else {
    UTIL_LOG(L3, (_T("[ValueWatcher::HasChanged]")
                  _T("[key name '%s'][value '%s' is the same]"),
                  key_watcher_->key_name(), value_name_));
  }

  return has_changed;
}

CString ValueWatcher::GetCurrentValueString() {
  CString value_data;
  RegKey& key = key_watcher_->key();
  ASSERT1(key.Key());
  value_is_valid_ = SUCCEEDED(key.GetValue(value_name_, &value_data));
  return value_is_valid_ ? value_data : CString();
}

DWORD ValueWatcher::GetCurrentValueDword() {
  DWORD value_data = 0;
  RegKey& key = key_watcher_->key();
  ASSERT1(key.Key());
  value_is_valid_ = SUCCEEDED(key.GetValue(value_name_, &value_data));
  return value_is_valid_ ? value_data : static_cast<DWORD>(-1);
}

void ValueWatcher::DoCallback() {
  ASSERT1(callback_ != NULL);

  const void* value = NULL;
  if (value_type_ == REG_SZ) {
    value = static_cast<const TCHAR*>(last_known_value_string_);
  } else if (value_type_ == REG_DWORD) {
    value = reinterpret_cast<void*>(last_known_value_dword_);
  }

  callback_(key_watcher_->key_name(), value_name_, change_type_,
            value, callback_param_);

  // If value was not valid, for example, the key was deleted or renamed, and
  // it is valid after callback, update last known with the current value.
  if (!value_is_valid_) {
    if (value_type_ == REG_SZ) {
      CString new_value = GetCurrentValueString();
      if (value_is_valid_) {
        last_known_value_string_ = new_value;
      }
    } else if (value_type_ == REG_DWORD) {
      DWORD new_value = GetCurrentValueDword();
      if (value_is_valid_) {
        last_known_value_dword_ = new_value;
      }
    }
  }
}

KeyWatcher::KeyWatcher(const KeyId& key_id)
    : key_id_(key_id),
      notification_event_(::CreateEvent(NULL, false, false, NULL)),
      callback_(NULL),
      callback_param_(NULL) {
}

KeyWatcher::~KeyWatcher() {
  for (size_t i = 0; i != values_.size(); ++i) {
    delete values_[i];
  }
}

HRESULT KeyWatcher::StartWatching() {
  // By this time the key could be deleted or renamed. Check if the handle
  // is still valid and reopen the key if needed.
  HRESULT hr = EnsureOpen();
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(key_.Key());
  const DWORD kNotifyFilter = REG_NOTIFY_CHANGE_NAME          |
                              REG_NOTIFY_CHANGE_ATTRIBUTES    |
                              REG_NOTIFY_CHANGE_LAST_SET      |
                              REG_NOTIFY_CHANGE_SECURITY;
  LONG result = ::RegNotifyChangeKeyValue(key_.Key(), false, kNotifyFilter,
                                          get(notification_event_), true);
  UTIL_LOG(L3, (_T("[KeyWatcher::StartWatching][key '%s' %s]"),
                key_id_.key_name(),
                result == ERROR_SUCCESS ? _T("ok") : _T("failed")));
  return HRESULT_FROM_WIN32(result);
}

HRESULT KeyWatcher::AddValue(const CString& value_name,
                             int value_type,
                             RegistryValueChangeCallback callback,
                             void* user_data) {
  ASSERT1(callback);
  HRESULT hr = EnsureOpen();
  if (FAILED(hr)) {
    return hr;
  }
  values_.push_back(
      new ValueWatcher(this, value_name, value_type, callback, user_data));
  return S_OK;
}

void KeyWatcher::HandleEvent(HANDLE handle) {
  UTIL_LOG(L3, (_T("[KeyWatcher::HandleEvent][key '%s']"), key_id_.key_name()));

  ASSERT1(handle);
  ASSERT1(handle == get(notification_event_));
  UNREFERENCED_PARAMETER(handle);

  // Although not documented, it seems the OS pulses the event so the event
  // is never signaled at this point.
  ASSERT1(::WaitForSingleObject(handle, 0) == WAIT_TIMEOUT);

  // Notify the key has changed.
  if (callback_) {
    callback_(key_name(), callback_param_);
  }

  // Notify the values have changed.
  for (size_t i = 0; i != values_.size(); ++i) {
    ValueWatcher* value = values_[i];
    if (value != NULL) {
      if (value->HasChanged()) {
        value->DoCallback();
      }
    }
  }

  VERIFY1(SUCCEEDED(StartWatching()));
}

HRESULT KeyWatcher::EnsureOpen() {
  // Close the key if it is not valid for whatever reasons, such as it was
  // deleted and recreated back.
  if (!IsKeyValid()) {
    UTIL_LOG(L3, (_T("[key '%s' is not valid]"), key_id_.key_name()));
    VERIFY1(SUCCEEDED(key_.Close()));
  }

  // Open the key if not already open or create the key if needed.
  HRESULT hr = S_OK;
  if (!key_.Key()) {
    hr = key_.Create(key_id_.parent_key(), key_id_.key_name());
    if (SUCCEEDED(hr)) {
      UTIL_LOG(L3, (_T("[key '%s' has been created]"), key_id_.key_name()));
    }
  }
  return hr;
}

bool KeyWatcher::IsKeyValid() {
  if (!key_.Key()) {
    return false;
  }
  LONG ret = RegQueryInfoKey(key_.Key(),
                             NULL, NULL, NULL, NULL, NULL,
                             NULL, NULL, NULL, NULL, NULL, NULL);
  return ret == ERROR_SUCCESS;
}

RegistryMonitorImpl::RegistryMonitorImpl() {
}

RegistryMonitorImpl::~RegistryMonitorImpl() {
  if (stop_monitoring_) {
    VERIFY1(::SetEvent(get(stop_monitoring_)));
  }
  VERIFY1(thread_.WaitTillExit(INFINITE));

  for (size_t i = 0; i != watchers_.size(); ++i) {
    ASSERT1(watchers_[i].second);
    delete watchers_[i].second;
  }
}

HRESULT RegistryMonitorImpl::Initialize() {
  reset(stop_monitoring_, ::CreateEvent(NULL, true, false, NULL));
  if (!stop_monitoring_) {
    return HRESULTFromLastError();
  }
  return S_OK;
}

HRESULT RegistryMonitorImpl::MonitorKey(HKEY root_key,
                                        const CString& sub_key,
                                        RegistryKeyChangeCallback callback,
                                        void* user_data) {
  ASSERT1(callback);
  ASSERT1(!thread_.Running());

  KeyId key_id(root_key, sub_key);
  for (size_t i = 0; i != watchers_.size(); ++i) {
    if (KeyId::IsEqual(watchers_[i].first, key_id)) {
      watchers_[i].second->set_callback(callback, user_data);
      return S_OK;
    }
  }
  scoped_ptr<KeyWatcher> key_watcher(new KeyWatcher(key_id));
  key_watcher->set_callback(callback, user_data);
  Watcher watcher(key_id, key_watcher.release());
  watchers_.push_back(watcher);
  return S_OK;
}

HRESULT RegistryMonitorImpl::MonitorValue(
    HKEY root_key, const CString& sub_key, const CString& value_name,
    int value_type, RegistryValueChangeCallback callback, void* user_data) {
  ASSERT1(callback);
  ASSERT1(!thread_.Running());

  // Reuse an existing key watcher if there is a value already registered
  // for monitoring under the respective registry key.
  KeyId key_id(root_key, sub_key);
  for (size_t i = 0; i != watchers_.size(); ++i) {
    if (KeyId::IsEqual(watchers_[i].first, key_id)) {
      return watchers_[i].second->AddValue(value_name, value_type,
                                           callback, user_data);
    }
  }
  scoped_ptr<KeyWatcher> key_watcher(new KeyWatcher(key_id));
  HRESULT hr = key_watcher->AddValue(value_name, value_type,
                                     callback, user_data);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[RegistryMonitorImpl::RegisterValue failed]")
                  _T("[key %s][value %s][0x%x]"), sub_key, value_name, hr));
    return hr;
  }
  Watcher watcher(key_id, key_watcher.release());
  watchers_.push_back(watcher);
  return S_OK;
}

HRESULT RegistryMonitorImpl::StartMonitoring() {
  // Starts the thread and waits on the gate for the thread to open after
  // it has registered all watchers for notifications and it is ready to
  // handle notification events. The gate is only needed to synchronize the
  // caller and the monitoring threads.
  start_monitoring_gate_.reset(new Gate);
  if (!thread_.Start(this)) {
    return E_FAIL;
  }
  bool wait_result = start_monitoring_gate_->Wait(INFINITE);
  start_monitoring_gate_.reset();
  ASSERT1(wait_result);
  return wait_result ? S_OK : HRESULTFromLastError();
}

void RegistryMonitorImpl::Run() {
  UTIL_LOG(L3, (_T("[started monitoring registry]")));

  const size_t kNumNotificationHandles = watchers_.size();
  const size_t kNumHandles = kNumNotificationHandles + 1;
  const size_t kStopMonitoringHandleIndex = kNumNotificationHandles;

  scoped_array<HANDLE> handles(new HANDLE[kNumHandles]);
  for (size_t i = 0; i != watchers_.size(); ++i) {
    handles[i] = watchers_[i].second->notification_event();
    VERIFY1(SUCCEEDED(watchers_[i].second->StartWatching()));
  }
  handles[kStopMonitoringHandleIndex] = get(stop_monitoring_);

  // Open the gate and allow the RegistryMonitor::StartMonitoring call to
  // to return to the caller.
  ASSERT1(start_monitoring_gate_.get());
  VERIFY1(start_monitoring_gate_->Open());

  for (;;) {
    DWORD result = ::WaitForMultipleObjects(kNumHandles,
                                            handles.get(),
                                            false,
                                            INFINITE);
    COMPILE_ASSERT(0 == WAIT_OBJECT_0, invalid_wait_object_0);
    ASSERT1(result < kNumHandles);
    if (result < kNumHandles) {
      if (result == kStopMonitoringHandleIndex) {
        break;
      } else {
        size_t i = result - WAIT_OBJECT_0;
        watchers_[i].second->HandleEvent(handles[i]);
      }
    }
  }
  UTIL_LOG(L3, (_T("[stopped monitoring registry]")));
}

}  // namespace detail

RegistryMonitor::RegistryMonitor()
    : impl_(new detail::RegistryMonitorImpl) {
}

RegistryMonitor::~RegistryMonitor() {
}

HRESULT RegistryMonitor::MonitorKey(HKEY root_key,
                                    const CString& sub_key,
                                    RegistryKeyChangeCallback callback,
                                    void* user_data) {
  return impl_->MonitorKey(root_key, sub_key, callback, user_data);
}

HRESULT RegistryMonitor::MonitorValue(HKEY root_key,
                                      const CString& sub_key,
                                      const CString& value_name,
                                      int value_type,
                                      RegistryValueChangeCallback callback,
                                      void* user_data) {
  return impl_->MonitorValue(root_key, sub_key, value_name, value_type,
                             callback, user_data);
}

HRESULT RegistryMonitor::Initialize() {
  return impl_->Initialize();
}

HRESULT RegistryMonitor::StartMonitoring() {
  return impl_->StartMonitoring();
}

}  // namespace omaha

