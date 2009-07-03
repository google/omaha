// Copyright 2005-2009 Google Inc.
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
// Base implementation and build blocks for NPAPI Plugins
// using the new NPRuntime supported by Firefox and others
//
// Mozilla doesn't currently have a framework in place for
// building plug-ins using the new NPRuntime model, so
// I had to roll my own.

#include "plugins/baseplugin.h"
#include "omaha/third_party/gecko/include/npupp.h"

BasePlugin::BasePlugin(NPP npp)
    : npp_(npp) {
  NPN_GetValue(npp_, NPNVWindowNPObject, &window_object_);
  NPN_RetainObject(window_object_);
}

BasePlugin::~BasePlugin() {
  if (window_object_) {
    NPN_ReleaseObject(window_object_);
  }
}

void BasePlugin::InitializePluginMap() {
  // Go through the list of IDs
  PLUGIN_MAP* pDispMap = GetPluginMap();
  PLUGIN_MAP_ENTRY* pEntry;

  if (pDispMap != NULL) {
    // populate all entries
    pEntry = pDispMap->entries;
    while (pEntry->name != NULL) {
      pEntry->np_id = NPN_GetStringIdentifier(pEntry->name);
      ++pEntry;
    }
  }
}

PLUGIN_MAP_ENTRY* BasePlugin::GetPluginEntry(NPIdentifier np_id)  {
  PLUGIN_MAP* pDispMap = GetPluginMap();
  PLUGIN_MAP_ENTRY* pEntry;

  if (pDispMap != NULL) {
    // search for entry
    pEntry = pDispMap->entries;
    while (pEntry->np_id != NULL) {
      if (pEntry->np_id == np_id) {
        return pEntry;
      }
      ++pEntry;
    }
  }

  return NULL;  // name not found
}

bool BasePlugin::HasMethod(NPIdentifier name)  {
  PLUGIN_MAP_ENTRY* pEntry = GetPluginEntry(name);
  return pEntry && !pEntry->function_get;
}

bool BasePlugin::InvokeDefault(const NPVariant *args,
                               uint32_t argument_count,
                               NPVariant *result) {
  UNREFERENCED_PARAMETER(args);
  UNREFERENCED_PARAMETER(argument_count);
  UNREFERENCED_PARAMETER(result);
  return false;
}


bool BasePlugin::Invoke(NPIdentifier name,
                        const NPVariant *args,
                        uint32_t argument_count,
                        NPVariant *result)  {
  // get entry for the member ID
  PLUGIN_MAP_ENTRY* pEntry = GetPluginEntry(name);
  if (pEntry == NULL || pEntry->function_get) {
    return false;
  }

  // do standard method call
  return (this->*(pEntry->function))(args, argument_count, result);
}

bool BasePlugin::HasProperty(NPIdentifier name)  {
  PLUGIN_MAP_ENTRY* pEntry = GetPluginEntry(name);
  return pEntry && pEntry->function_get;
}

bool BasePlugin::GetProperty(NPIdentifier name, NPVariant *result) {
  PLUGIN_MAP_ENTRY* pEntry = GetPluginEntry(name);
  if (pEntry == NULL || !pEntry->function_get) {
    return false;
  }

  // do standard method call
  return (this->*(pEntry->function_get))(result);
}

bool BasePlugin::SetProperty(NPIdentifier name,
                             const NPVariant *value) {
  // get entry for the member ID
  PLUGIN_MAP_ENTRY* pEntry = GetPluginEntry(name);
  if (pEntry == NULL || !pEntry->function_get) {
    return false;
  }

  // do standard method call
  return (this->*(pEntry->function_set))(value);
}


void BasePlugin::Invalidate() {
}

bool BasePlugin::RemoveProperty(NPIdentifier name)  {
  UNREFERENCED_PARAMETER(name);
  return false;
}

void BasePlugin::Shutdown() {
}

// static
void BasePlugin::_Deallocate(NPObject *npobj) {
  // Call the virtual destructor.
  delete static_cast<BasePlugin *>(npobj);
}

// static
void BasePlugin::_Invalidate(NPObject *npobj) {
  (static_cast<BasePlugin *>(npobj))->Invalidate();
}

// static
bool BasePlugin::_HasMethod(NPObject *npobj, NPIdentifier name)  {
  return (static_cast<BasePlugin *>(npobj))->HasMethod(name);
}

// static
bool BasePlugin::_Invoke(NPObject *npobj,
                         NPIdentifier name,
                         const NPVariant *args,
                         uint32_t argument_count,
                         NPVariant *result)  {
  return (static_cast<BasePlugin *>(npobj))->Invoke(
      name,
      args,
      argument_count,
      result);
}

// static
bool BasePlugin::_InvokeDefault(NPObject *npobj,
                                const NPVariant *args,
                                uint32_t argument_count,
                                NPVariant *result) {
  return (static_cast<BasePlugin *>(npobj))->InvokeDefault(
      args,
      argument_count,
      result);
}

// static
bool BasePlugin::_HasProperty(NPObject * npobj, NPIdentifier name)  {
  return (static_cast<BasePlugin *>(npobj))->HasProperty(name);
}

// static
bool BasePlugin::_GetProperty(NPObject *npobj,
                              NPIdentifier name,
                              NPVariant *result) {
  return (static_cast<BasePlugin *>(npobj))->GetProperty(name, result);
}

// static
bool BasePlugin::_SetProperty(NPObject *npobj,
                              NPIdentifier name,
                              const NPVariant *value)  {
  return (static_cast<BasePlugin *>(npobj))->SetProperty(name, value);
}

// static
bool BasePlugin::_RemoveProperty(NPObject *npobj, NPIdentifier name)  {
  return (static_cast<BasePlugin *>(npobj))->RemoveProperty(name);
}

CPlugin::CPlugin(NPP pNPInstance)
    : np_instance_(pNPInstance),
      bInitialized_(FALSE),
      scriptable_object_(NULL)  {
}

CPlugin::~CPlugin() {
  if (scriptable_object_) {
    NPN_ReleaseObject(scriptable_object_);
  }
}

NPBool CPlugin::init(NPWindow* np_window) {
  bInitialized_ = TRUE;
  NPBool returnvalue = TRUE;
  if (the_npclass_staticinit) {
      returnvalue = (*the_npclass_staticinit)
          (np_instance_, np_window)? TRUE : FALSE;
  }

  return returnvalue;
}

void CPlugin::shut()  {
  if (scriptable_object_) {
    (static_cast<BasePlugin*>(scriptable_object_))->Shutdown();
  }
  bInitialized_ = FALSE;
}

NPBool CPlugin::isInitialized() {
  return bInitialized_;
}

int16 CPlugin::handleEvent(void* event) {
  UNREFERENCED_PARAMETER(event);
  return 0;
}

NPObject* CPlugin::GetScriptableObject()  {
  if (!scriptable_object_) {
    scriptable_object_ = NPN_CreateObject(
        np_instance_,
        &CPlugin::the_npclass);
  }

  if (scriptable_object_) {
    NPN_RetainObject(scriptable_object_);
  }

  return scriptable_object_;
}

