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
// LegacyManifestHandler monitors the user manifest directory for manifest
// files that are copied there by omaha1.

#ifndef OMAHA_CORE_LEGACY_MANIFEST_HANDLER_H__
#define OMAHA_CORE_LEGACY_MANIFEST_HANDLER_H__

#include <windows.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/event_handler.h"
#include "omaha/common/file.h"

namespace omaha {

class Reactor;
class Core;

class LegacyManifestHandler : public EventHandler {
 public:
  LegacyManifestHandler();
  ~LegacyManifestHandler();

  HRESULT Initialize(Core* core);

  virtual void HandleEvent(HANDLE handle);

 private:
  scoped_ptr<FileWatcher> dir_watcher_;
  Reactor* reactor_;
  Core* core_;

  DISALLOW_EVIL_CONSTRUCTORS(LegacyManifestHandler);
};

}  // namespace omaha

#endif  // OMAHA_CORE_LEGACY_MANIFEST_HANDLER_H__

