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
//
// TODO(omaha) - better document this code so other Google
// projects can use this framework.

#ifndef OMAHA_PLUGINS_BASEPLUGIN_H__
#define OMAHA_PLUGINS_BASEPLUGIN_H__

#include <windows.h>
#include <windowsx.h>
#include "omaha/third_party/gecko/include/npapi.h"
#include "omaha/third_party/gecko/include/npruntime.h"

class BasePlugin;
typedef bool (BasePlugin::*PLUGIN_FUNC)(
  const NPVariant *args, uint32_t argument_count, NPVariant *result);
typedef bool (BasePlugin::*PLUGIN_PROPGET)(NPVariant *result);
typedef bool (BasePlugin::*PLUGIN_PROPSET)(const NPVariant *value);
typedef NPBool (*PLUGIN_STATIC_INIT)(NPP np_instance, NPWindow* np_window);

struct PLUGIN_MAP_ENTRY {
  LPCSTR name;                  // member/property name
  NPIdentifier np_id;           // NPIdentifier
  PLUGIN_FUNC function;         // member Function
  PLUGIN_PROPGET function_get;  // member for Get<property>
  PLUGIN_PROPSET function_set;  // member for Set<property>
  uint32_t argument_count;      // argument_count for Invoke()
};

struct PLUGIN_MAP {
  PLUGIN_MAP_ENTRY* entries;
};

class CPlugin {
 private:
  NPP np_instance_;
  NPBool bInitialized_;
  NPObject *scriptable_object_;

 public:
  explicit CPlugin(NPP pNPInstance);
  ~CPlugin();

  NPBool init(NPWindow* np_window);
  void shut();
  NPBool isInitialized();

  int16 handleEvent(void* event);

  NPObject *GetScriptableObject();
  static NPClass the_npclass;
  static PLUGIN_STATIC_INIT the_npclass_staticinit;
};



// Helper class that can be used to map calls to the NPObject hooks
// into virtual methods on instances of classes that derive from this
// class.
class BasePlugin : public NPObject  {
 public:
  explicit BasePlugin(NPP npp);
  virtual ~BasePlugin();

  // Virtual functions that a derived class can override
  virtual void Invalidate();
  virtual bool HasMethod(NPIdentifier name);
  virtual bool Invoke(NPIdentifier name, const NPVariant *args,
                      uint32_t argument_count, NPVariant *result);
  virtual bool InvokeDefault(const NPVariant *args, uint32_t argument_count,
                             NPVariant *result);
  virtual bool HasProperty(NPIdentifier name);
  virtual bool GetProperty(NPIdentifier name, NPVariant *result);
  virtual bool SetProperty(NPIdentifier name, const NPVariant *value);
  virtual bool RemoveProperty(NPIdentifier name);
  virtual void Shutdown();

  virtual PLUGIN_MAP_ENTRY* GetPluginEntry(NPIdentifier np_id);

 protected:
  // Virtual functions that a derived class can override
  virtual PLUGIN_MAP* GetPluginMap() {return NULL;}

  // Helpers:
  void InitializePluginMap();
  NPObject* window_object() {return window_object_;}

 public:
  // Statics
  static void _Deallocate(NPObject *npobj);
  static void _Invalidate(NPObject *npobj);
  static bool _HasMethod(NPObject *npobj, NPIdentifier name);
  static bool _Invoke(NPObject *npobj, NPIdentifier name,
                      const NPVariant *args, uint32_t argument_count,
                      NPVariant *result);
  static bool _InvokeDefault(NPObject *npobj, const NPVariant *args,
                             uint32_t argument_count, NPVariant *result);
  static bool _HasProperty(NPObject * npobj, NPIdentifier name);
  static bool _GetProperty(NPObject *npobj, NPIdentifier name,
                           NPVariant *result);
  static bool _SetProperty(NPObject *npobj, NPIdentifier name,
                           const NPVariant *value);
  static bool _RemoveProperty(NPObject *npobj, NPIdentifier name);

 protected:
  NPP npp_;
  NPObject *window_object_;
};

// Only one of these can be declared:
#define DECLARE_CPLUGIN_NPCLASS() \
  static NPBool init(NPP np_instance, NPWindow* np_window);

#define DEFINE_CPLUGIN_NPCLASS(the_class) \
  NPClass CPlugin::the_npclass = the_class::get_NPClass(); \
  PLUGIN_STATIC_INIT CPlugin::the_npclass_staticinit = the_class::init; \
  NPBool the_class::init(NPP np_instance, NPWindow* np_window)  {            \
    UNREFERENCED_PARAMETER(np_instance);                                     \
    UNREFERENCED_PARAMETER(np_window);                                       \

#define END_DEFINE_CPLUGIN_NPCLASS() \
  }

#define DECLARE_PLUGIN_CLASS(the_class)                                      \
class the_class : public BasePlugin {                                        \
private:                                                                     \
  static PLUGIN_MAP_ENTRY plugin_entries_[];                                 \
  static NPClass the_class##_NPClass;                                        \
protected:                                                                   \
  virtual PLUGIN_MAP* GetPluginMap();                                        \
  static PLUGIN_MAP plugin_map;                                              \
  static NPObject* AllocateScriptablePluginObject(NPP npp,                   \
                       NPClass *theclass) {                                  \
    UNREFERENCED_PARAMETER(theclass);                                        \
    return new the_class(npp);                                               \
  }                                                                          \
public:                                                                      \
  static NPClass& get_NPClass() {return the_class##_NPClass;}                \
  explicit the_class(NPP npp);                                               \
  virtual ~the_class();                                                      \

#define DECLARE_PLUGIN_FUNCTION(member_function)                             \
  bool member_function(const NPVariant *args,                                \
           uint32_t argument_count, NPVariant *result);

#define DECLARE_PLUGIN_PROPERTY(member_function)     \
  bool Get##member_function(NPVariant *result);      \
  bool Set##member_function(const NPVariant *value);

#define END_DECLARE_PLUGIN_CLASS(the_class)    \
  };

// Macros to define plugin methods
#define BEGIN_PLUGIN_CLASS(the_class)                         \
  NPClass the_class::the_class##_NPClass = {                  \
  NP_CLASS_STRUCT_VERSION,                                    \
  AllocateScriptablePluginObject,                             \
  BasePlugin::_Deallocate,                                    \
  BasePlugin::_Invalidate,                                    \
  BasePlugin::_HasMethod,                                     \
  BasePlugin::_Invoke,                                        \
  BasePlugin::_InvokeDefault,                                 \
  BasePlugin::_HasProperty,                                   \
  BasePlugin::_GetProperty,                                   \
  BasePlugin::_SetProperty,                                   \
  BasePlugin::_RemoveProperty                                 \
};                                                            \
PLUGIN_MAP* the_class::GetPluginMap() {                       \
  static bool initialized = false;                            \
  if (!initialized) {                                         \
    initialized = true;                                       \
    InitializePluginMap();                                    \
  }                                                           \
  return &the_class::plugin_map;                              \
  }                                                           \
PLUGIN_MAP the_class::plugin_map =                            \
  { &the_class::plugin_entries_[0] };                         \
PLUGIN_MAP_ENTRY the_class::plugin_entries_[] =  {            \
                                                                    // NO_LINT
#define PLUGIN_FUNCTION(the_class, member_function, argument_count)       \
  { #member_function, NULL,                                               \
    (PLUGIN_FUNC)&member_function, NULL, NULL, argument_count             \
    },

#define PLUGIN_PROPERTY(the_class, member_function)                       \
  { #member_function, NULL,                                               \
  NULL, (PLUGIN_PROPGET)&Get##member_function,                            \
  (PLUGIN_PROPSET)&Set##member_function,                                  \
    0},

#define END_BEGIN_PLUGIN_CLASS \
  { NULL, NULL,                \
    NULL, NULL, NULL, 0,       \
    }, };

#define BEGIN_PLUGIN_CLASS_CTOR(the_class)                                   \
  the_class::the_class(NPP npp) : BasePlugin(npp) {                          \
                                                                    // NO_LINT
#define END_PLUGIN_CLASS_CTOR                                                \
  }

#define BEGIN_PLUGIN_CLASS_DTOR(the_class)                                   \
  the_class::~the_class() {                                                  \
                                                                    // NO_LINT
#define END_PLUGIN_CLASS_DTOR                                                \
  }

#endif  // OMAHA_PLUGINS_BASEPLUGIN_H__

