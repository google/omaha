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

#include "omaha/core/legacy_manifest_handler.h"

#include <atlsecurity.h>
#include <atlstr.h>
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/reactor.h"
#include "omaha/core/core.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"

namespace omaha {

LegacyManifestHandler::LegacyManifestHandler()
    : core_(NULL),
      reactor_(NULL) {
  CORE_LOG(L3, (_T("[LegacyManifestHandler::LegacyManifestHandler]")));
}

LegacyManifestHandler::~LegacyManifestHandler() {
  CORE_LOG(L3, (_T("[LegacyManifestHandler::~LegacyManifestHandler]")));
  ASSERT1(reactor_);
  ASSERT1(dir_watcher_.get());
  VERIFY1(SUCCEEDED(reactor_->UnregisterHandle(dir_watcher_->change_event())));
}

HRESULT LegacyManifestHandler::Initialize(Core* core) {
  CORE_LOG(L3, (_T("[LegacyManifestHandler::Initialize]")));
  ASSERT1(core);
  ASSERT1(!core->is_system());
  core_ = core;

  reactor_ = core_->reactor();
  ASSERT1(reactor_);

  CString dir = ConfigManager::Instance()->GetUserInitialManifestStorageDir();
  dir_watcher_.reset(new FileWatcher(dir,
                                     false,
                                     FILE_NOTIFY_CHANGE_FILE_NAME));
  ASSERT1(dir_watcher_.get());
  HRESULT hr = dir_watcher_->EnsureEventSetup();
  if (FAILED(hr)) {
    return hr;
  }

  hr = reactor_->RegisterHandle(dir_watcher_->change_event(), this, 0);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

void LegacyManifestHandler::HandleEvent(HANDLE handle) {
  UNREFERENCED_PARAMETER(handle);
  ASSERT1(handle == dir_watcher_->change_event());
  CORE_LOG(L1, (_T("[Got an omaha1 manifest handoff event.]")));

  // Set up the watcher again.
  ASSERT1(dir_watcher_.get());
  HRESULT hr = dir_watcher_->EnsureEventSetup();
  if (FAILED(hr)) {
    // Ignore the error and handle the current event.
    // It is possible to not be able to register the file watcher in case
    // omaha is being uninstalled.
    CORE_LOG(L1, (_T("Could not recreate the file watcher")));
  }
  VERIFY1(SUCCEEDED(reactor_->RegisterHandle(dir_watcher_->change_event())));

  ASSERT1(core_);
  core_->StartInstallWorker();
}

}  // namespace omaha

