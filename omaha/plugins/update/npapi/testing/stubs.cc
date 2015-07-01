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

#include "omaha/plugins/update/npapi/testing/stubs.h"
#include <malloc.h>
#include <string.h>
#include "base/debug.h"

namespace omaha {

NPIdentifierFactory::NPIdentifierFactory() {
}

NPIdentifierFactory::~NPIdentifierFactory() {
  for (std::vector<NPIdentifier>::const_iterator it = identifiers_.begin();
       it != identifiers_.end(); ++it) {
    free(*it);
  }
}

NPIdentifier NPIdentifierFactory::Create(const char* name) {
  NPIdentifier identifier = _strdup(name);
  identifiers_.push_back(identifier);
  return identifier;
}

}  // namespace omaha

extern "C" {
void* NPN_MemAlloc(uint32 size) {
  return malloc(size);
}

void NPN_MemFree(void* ptr) {
  free(ptr);
}

NPUTF8* NPN_UTF8FromIdentifier(NPIdentifier identifier) {
  return _strdup(static_cast<char*>(identifier));
}

NPObject* NPN_CreateObject(NPP npp, NPClass* class_vtable) {
  UNREFERENCED_PARAMETER(npp);
  ASSERT1(class_vtable);
  NPObject* object = class_vtable->allocate(npp, class_vtable);
  object->_class = class_vtable;
  object->referenceCount = 1;
  return object;
}

NPObject* NPN_RetainObject(NPObject* object) {
  ASSERT1(object);
  ++object->referenceCount;
  return object;
}

void NPN_ReleaseObject(NPObject* object) {
  ASSERT1(object);
  ASSERT1(object->referenceCount > 0);
  if (--object->referenceCount == 0) {
    object->_class->deallocate(object);
  }
}

void NPN_ReleaseVariantValue(NPVariant* variant) {
  if (NPVARIANT_IS_STRING(*variant)) {
    NPN_MemFree(const_cast<NPUTF8*>(variant->value.stringValue.UTF8Characters));
  } else if (NPVARIANT_IS_OBJECT(*variant)) {
    NPN_ReleaseObject(variant->value.objectValue);
  }
  VOID_TO_NPVARIANT(*variant);
  return;
}

bool NPN_HasMethod(NPP npp, NPObject* object, NPIdentifier name) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->hasMethod(object, name);
}

bool NPN_Invoke(NPP npp, NPObject* object, NPIdentifier name,
                const NPVariant* args, uint32_t arg_count, NPVariant* result) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->invoke(object, name, args, arg_count, result);
}

bool NPN_InvokeDefault(NPP npp, NPObject* object, const NPVariant* args,
                       uint32_t arg_count, NPVariant* result) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->invokeDefault(object, args, arg_count, result);
}

bool NPN_HasProperty(NPP npp, NPObject* object, NPIdentifier name) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->hasProperty(object, name);
}

bool NPN_GetProperty(NPP npp, NPObject* object, NPIdentifier name,
                     NPVariant* result) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->getProperty(object, name, result);
}

bool NPN_SetProperty(NPP npp, NPObject* object, NPIdentifier name,
                     const NPVariant* value) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->setProperty(object, name, value);
}

bool NPN_RemoveProperty(NPP npp, NPObject* object, NPIdentifier name) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->removeProperty(object, name);
}

bool NPN_Enumerate(NPP npp, NPObject* object, NPIdentifier** names,
                   uint32_t* count) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->enumerate(object, names, count);
}

bool NPN_Construct(NPP npp, NPObject* object, const NPVariant* args,
                   uint32_t arg_count, NPVariant* result) {
  UNREFERENCED_PARAMETER(npp);
  return object->_class->construct(object, args, arg_count, result);
}

void NPN_SetException(NPObject* object, const NPUTF8* message) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(message);
}
}  // extern "C"
