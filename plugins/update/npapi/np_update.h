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

#ifndef OMAHA_PLUGINS_UPDATE_NPAPI_NP_UPDATE_H_
#define OMAHA_PLUGINS_UPDATE_NPAPI_NP_UPDATE_H_

#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/plugins/base/pluginbase.h"
#include "omaha/plugins/update/site_lock.h"
#include "third_party/npapi/bindings/nphostapi.h"

namespace omaha {

class DispatchHost;

class NPUpdate : public nsPluginInstanceBase {
 public:
  explicit NPUpdate(NPP instance, const char* mime_type);
  virtual ~NPUpdate();

  // nsPluginInstanceBase overrides.
  virtual NPBool init(NPWindow* np_window);
  virtual void shut();
  virtual NPBool isInitialized();
  virtual NPError GetValue(NPPVariable variable, void* value);

 private:
  bool GetCurrentBrowserUrl(CString* url);
  bool MapMimeTypeToClsid(CLSID* clsid);

  NPP instance_;
  bool is_initialized_;
  CString mime_type_;
  SiteLock site_lock_;
  DispatchHost* scriptable_object_;

  friend class NPUpdateTest;

  DISALLOW_COPY_AND_ASSIGN(NPUpdate);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_NPAPI_NP_UPDATE_H_
