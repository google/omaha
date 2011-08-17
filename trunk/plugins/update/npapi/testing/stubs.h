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
//
// Normally, implementations of these functions are provided by the NPAPI
// runtime. These stub implementations are intended only for use in unit tests.

#ifndef OMAHA_PLUGINS_UPDATE_NPAPI_TESTING_STUBS_H_
#define OMAHA_PLUGINS_UPDATE_NPAPI_TESTING_STUBS_H_

#include <vector>
#include "third_party/npapi/bindings/nphostapi.h"

namespace omaha {

// Not part of NPAPI proper, but useful nonetheless. Note that the stub
// implementation of NPIdentifier does not conform to NPAPI's idea of what an
// NPIdentifier ought to be: specifically, uniqueness is not guaranteed.
class NPIdentifierFactory {
 public:
  NPIdentifierFactory();
  ~NPIdentifierFactory();
  NPIdentifier Create(const char* name);

 private:
  std::vector<NPIdentifier> identifiers_;

  DISALLOW_COPY_AND_ASSIGN(NPIdentifierFactory);
};

}  // namespace omaha

extern "C" {
void* NPN_MemAlloc(uint32 size);
void NPN_MemFree(void* ptr);
NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier identifier);
NPObject* NPN_CreateObject(NPP npp, NPClass* class_vtable);
NPObject* NPN_RetainObject(NPObject* object);
void NPN_ReleaseObject(NPObject* object);
void NPN_ReleaseVariantValue(NPVariant* variant);

bool NPN_HasMethod(NPP npp, NPObject* object, NPIdentifier name);
bool NPN_Invoke(NPP npp, NPObject* object, NPIdentifier name,
                const NPVariant* args, uint32_t arg_count, NPVariant* result);
bool NPN_InvokeDefault(NPP npp, NPObject* object, const NPVariant* args,
                       uint32_t arg_count, NPVariant* result);
bool NPN_HasProperty(NPP npp, NPObject* object, NPIdentifier name);
bool NPN_GetProperty(NPP npp, NPObject* object, NPIdentifier name,
                     NPVariant* result);
bool NPN_SetProperty(NPP npp, NPObject* object, NPIdentifier name,
                     const NPVariant* value);
bool NPN_RemoveProperty(NPP npp, NPObject* object, NPIdentifier name);
bool NPN_Enumerate(NPP npp, NPObject* object, NPIdentifier** names,
                   uint32_t* count);
bool NPN_Construct(NPP npp, NPObject* object, const NPVariant* args,
                   uint32_t arg_count, NPVariant* result);
void NPN_SetException(NPObject* object, const NPUTF8* message);
}  // extern "C"

#endif  // OMAHA_PLUGINS_UPDATE_NPAPI_TESTING_STUBS_H_

