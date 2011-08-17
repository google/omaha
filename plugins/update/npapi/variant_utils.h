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
// Converts between NPVariant and VARIANT types.
// The following two-way conversions are supported:
// NPVariantType_Void <-> VT_EMPTY
// NPVariantType_Null <-> VT_NULL
// NPVariantType_Bool <-> VT_BOOL
// NPVariantType_Int32 <-> VT_I4
// NPVariantType_Double <-> VT_R8
// NPVariantType_String <-> VT_BSTR
//
// Furthermore, the following one-way conversions are supported:
// VT_UI4 -> NPVariantType_Int32
// VT_DISPATCH -> NPVariantType_Object (DispatchHost)
// NPVariantType_Object -> VT_DISPATCH (NpFunctionHost)

#ifndef OMAHA_PLUGINS_UPDATE_NPAPI_VARIANT_UTILS_H_
#define OMAHA_PLUGINS_UPDATE_NPAPI_VARIANT_UTILS_H_

#include <oaidl.h>
#include "third_party/npapi/bindings/nphostapi.h"

namespace omaha {

void NPVariantToVariant(NPP npp,
                        const NPVariant& source,
                        VARIANT* destination);

void VariantToNPVariant(NPP npp,
                        const VARIANT& source,
                        NPVariant* destination);

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_NPAPI_VARIANT_UTILS_H_
