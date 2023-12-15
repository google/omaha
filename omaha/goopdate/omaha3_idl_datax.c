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
// Wrapper for the MIDL-generated proxy/stub.

#pragma warning(push)
// C4152: nonstandard extension, function/data pointer conversion in expression
// C2251: code_seg changed after including header
#pragma warning(disable : 4152 5251)

#define REGISTER_PROXY_DLL
#define USE_STUBLESS_PROXY
#define PROXY_DELEGATION
#define ENTRY_PREFIX      Prx

// PROXY_CLSID_IS_MACHINE/USER is defined in main.scons.
#undef PROXY_CLSID_IS
#if IS_MACHINE_HANDLER
  #define PROXY_CLSID_IS  PROXY_CLSID_IS_MACHINE
#else
  #define PROXY_CLSID_IS  PROXY_CLSID_IS_USER
#endif

// Undefine the __purecall provided by rpcproxy.h so that it does not conflict
// with the libc definition.
#include <rpcproxy.h>
#ifdef DLLDUMMYPURECALL
#undef DLLDUMMYPURECALL
#define DLLDUMMYPURECALL
#endif

#ifndef _M_AMD64
  #include "goopdate/omaha3_idl_data.c"
  #include "goopdate/omaha3_idl_p.c"
#else
  #include "goopdate/omaha3_idl_64_data.c"
  #include "goopdate/omaha3_idl_64_p.c"
#endif

#undef PROXY_CLSID_IS

#pragma warning(pop)

