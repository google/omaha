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
// TODO(omaha): verify that the new operator throws on failure to prevent NULL
// pointer exploits.

#include "omaha/plugins/update/npapi/variant_utils.h"

#include <stdint.h>

#include <atlstr.h>
#include "base/debug.h"
#include "base/string.h"
#include "omaha/plugins/update/npapi/dispatch_host.h"
#include "omaha/plugins/update/npapi/npfunction_host.h"

namespace omaha {

void NPVariantToVariant(NPP npp,
                        const NPVariant& source,
                        VARIANT* destination) {
  ASSERT1(destination);
  V_VT(destination) = VT_EMPTY;

  switch (source.type) {
    case NPVariantType_Void:
      V_VT(destination) = VT_EMPTY;
      break;
    case NPVariantType_Null:
      V_VT(destination) = VT_NULL;
      break;
    case NPVariantType_Bool:
      V_VT(destination) = VT_BOOL;
      V_BOOL(destination) = source.value.boolValue ? VARIANT_TRUE
                                                   : VARIANT_FALSE;
      break;
    case NPVariantType_Int32:
      V_VT(destination) = VT_I4;
      V_I4(destination) = source.value.intValue;
      break;
    case NPVariantType_Double:
      V_VT(destination) = VT_R8;
      V_R8(destination) = source.value.doubleValue;
      break;
    case NPVariantType_String:
      V_VT(destination) = VT_BSTR;
      if (source.value.stringValue.UTF8Length) {
        CString string = Utf8ToWideChar(source.value.stringValue.UTF8Characters,
                                        source.value.stringValue.UTF8Length);
        V_BSTR(destination) = string.AllocSysString();
      } else {
        V_BSTR(destination) = CString().AllocSysString();
      }
      break;
    case NPVariantType_Object:
      V_VT(destination) = VT_DISPATCH;
      if (source.value.objectValue) {
        NpFunctionHost::Create(npp, source.value.objectValue,
                               &V_DISPATCH(destination));
      } else {
        V_DISPATCH(destination) = NULL;
      }
      break;
    default:
      ASSERT1(false);
      break;
  }
}

void VariantToNPVariant(NPP npp,
                        const VARIANT& source,
                        NPVariant* destination) {
  ASSERT1(destination);
  VOID_TO_NPVARIANT(*destination);

  switch (V_VT(&source)) {
    case VT_EMPTY:
      VOID_TO_NPVARIANT(*destination);
      break;
    case VT_NULL:
      NULL_TO_NPVARIANT(*destination);
      break;
    case VT_BOOL:
      BOOLEAN_TO_NPVARIANT(V_BOOL(&source), *destination);
      break;
    case VT_I4:
      INT32_TO_NPVARIANT(V_I4(&source), *destination);
      break;
    case VT_UI4:
      INT32_TO_NPVARIANT(V_UI4(&source), *destination);
      break;
    case VT_R8:
      DOUBLE_TO_NPVARIANT(V_R8(&source), *destination);
      break;
    case VT_BSTR:
      if (V_BSTR(&source)) {
        int source_length = ::SysStringLen(V_BSTR(&source)) + 1;
        int buffer_length = ::WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&source),
                                                  source_length, NULL, 0, NULL,
                                                  NULL);
        if (buffer_length == 0) {
          break;
        }
        char* buffer = static_cast<char*>(NPN_MemAlloc(buffer_length));
        VERIFY1(::WideCharToMultiByte(CP_UTF8, 0, V_BSTR(&source),
                                      source_length, buffer, buffer_length,
                                      NULL, NULL) > 0);
        STRINGN_TO_NPVARIANT(buffer,
                             static_cast<uint32_t>(buffer_length - 1),
                             *destination);
      } else {
        char* buffer = static_cast<char*>(NPN_MemAlloc(1));
        buffer[0] = '\0';
        STRINGN_TO_NPVARIANT(buffer, 0, *destination);
      }
      break;
    case VT_DISPATCH:
      if (V_DISPATCH(&source)) {
        OBJECT_TO_NPVARIANT(
            DispatchHost::CreateInstance(npp, V_DISPATCH(&source)),
            *destination);
      }
      break;
    default:
      ASSERT(false, (L"Unhandled variant type %d", V_VT(&source)));
      break;
  }
}

}  // namespace omaha
