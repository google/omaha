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

#include "omaha/plugins/update/npapi/np_update.h"

#include <atlbase.h>
#include <atlcom.h>

#include "omaha/base/debug.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/string.h"
#include "omaha/plugins/update/config.h"
#include "omaha/plugins/update/npapi/dispatch_host.h"
#include "omaha/plugins/update/npapi/urlpropbag.h"
#include "plugins/update/activex/update_control_idl.h"

NPError NS_PluginInitialize() {
  return NPERR_NO_ERROR;
}

void NS_PluginShutdown() {
}

nsPluginInstanceBase* NS_NewPluginInstance(nsPluginCreateData* data) {
  return new omaha::NPUpdate(data->instance, data->type);
}

void NS_DestroyPluginInstance(nsPluginInstanceBase* plugin) {
  delete plugin;
}

namespace omaha {

NPUpdate::NPUpdate(NPP instance, const char* mime_type)
    : instance_(instance),
      is_initialized_(false),
      mime_type_(mime_type),
      scriptable_object_(NULL) {
  ASSERT1(instance);
  // TODO(omaha): initialize COM
}

NPUpdate::~NPUpdate() {
  if (scriptable_object_) {
    NPN_ReleaseObject(scriptable_object_);
  }
}

NPBool NPUpdate::init(NPWindow* np_window) {
  UNREFERENCED_PARAMETER(np_window);
  is_initialized_ = true;
  return TRUE;
}

void NPUpdate::shut() {
  is_initialized_ = false;
}

NPBool NPUpdate::isInitialized() {
  // TODO(omaha): figure the right boolean type to return here...
  return is_initialized_ ? TRUE : FALSE;
}

NPError NPUpdate::GetValue(NPPVariable variable, void* value) {
  if (!instance_) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  if (NPPVpluginScriptableNPObject != variable || !value) {
    return NPERR_INVALID_PARAM;
  }

  CString url;
  if (!GetCurrentBrowserUrl(&url) || !site_lock_.InApprovedDomain(url)) {
    return NPERR_INVALID_URL;
  }

  if (!scriptable_object_) {
    CComPtr<IDispatch> p;

    CLSID clsid;
    if (!MapMimeTypeToClsid(&clsid)) {
      return NPERR_INVALID_PLUGIN_ERROR;
    }
    if (FAILED(p.CoCreateInstance(clsid))) {
      return NPERR_OUT_OF_MEMORY_ERROR;
    }

    // Store the current URL in a property bag and set it as the site of
    // the object.
    CComPtr<IPropertyBag> pb;
    if (FAILED(UrlPropertyBag::Create(url, &pb))) {
      return NPERR_GENERIC_ERROR;
    }
    CComPtr<IObjectWithSite> sited_obj;
    if (FAILED(p.QueryInterface(&sited_obj))) {
      return NPERR_GENERIC_ERROR;
    }
    if (FAILED(sited_obj->SetSite(pb))) {
      return NPERR_GENERIC_ERROR;
    }

    scriptable_object_ = DispatchHost::CreateInstance(instance_, p);
  }

  if (scriptable_object_) {
    NPN_RetainObject(scriptable_object_);
  } else {
    return NPERR_OUT_OF_MEMORY_ERROR;
  }

  *(reinterpret_cast<NPObject**>(value)) = scriptable_object_;
  return NPERR_NO_ERROR;
}

bool NPUpdate::MapMimeTypeToClsid(CLSID* clsid) {
  ASSERT1(clsid);
  // TODO(omaha): We could probably abstract this out to a map that can
  // have entries added to it at runtime, making this module fully generic.
  // We could also consider extracting the MIME_TYPE resource from the current
  // DLL and populating it from that.
  if (0 == mime_type_.CompareNoCase(CString(UPDATE3WEB_MIME_TYPE))) {
    *clsid = __uuidof(GoogleUpdate3WebControlCoClass);
    return true;
  }
  if (0 == mime_type_.CompareNoCase(CString(ONECLICK_MIME_TYPE))) {
    *clsid = __uuidof(GoogleUpdateOneClickControlCoClass);
    return true;
  }
  return false;
}

bool NPUpdate::GetCurrentBrowserUrl(CString* url) {
  ASSERT1(url);

  NPObject* window = NULL;
  NPError error = NPN_GetValue(instance_, NPNVWindowNPObject, &window);
  if (NPERR_NO_ERROR != error || !window) {
    ASSERT(false, (L"NPN_GetValue returned error %d", error));
    return false;
  }
  ON_SCOPE_EXIT(NPN_ReleaseObject, window);

  NPIdentifier location_id = NPN_GetStringIdentifier("location");
  NPVariant location;
  NULL_TO_NPVARIANT(location);
  if (!NPN_GetProperty(instance_, window, location_id, &location)) {
    ASSERT1(false);
    return false;
  }
  ON_SCOPE_EXIT(NPN_ReleaseVariantValue, &location);
  if (!NPVARIANT_IS_OBJECT(location)) {
    ASSERT(false, (L"Variant type: %d", location.type));
    return false;
  }

  NPIdentifier href_id = NPN_GetStringIdentifier("href");
  NPVariant href;
  NULL_TO_NPVARIANT(href);
  if (!NPN_GetProperty(instance_, NPVARIANT_TO_OBJECT(location), href_id,
                       &href)) {
    ASSERT1(false);
    return false;
  }
  ON_SCOPE_EXIT(NPN_ReleaseVariantValue, &href);
  if (!NPVARIANT_IS_STRING(href)) {
    ASSERT(false, (L"Variant type: %d", href.type));
    return false;
  }

  *url = Utf8ToWideChar(NPVARIANT_TO_STRING(href).UTF8Characters,
                        NPVARIANT_TO_STRING(href).UTF8Length);
  return true;
}

}  // namespace omaha
