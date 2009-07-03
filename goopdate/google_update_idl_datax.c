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
// google_update_idl_datax.c: Wrapper for the MIDL-generated proxy/stub.

#pragma warning(push)
// C4255: no function prototype given: converting '()' to '(void)'
#pragma warning(disable : 4255)
// C4152: nonstandard extension, function/data pointer conversion in expression
#pragma warning(disable : 4152)

#define REGISTER_PROXY_DLL
#define USE_STUBLESS_PROXY
#define ENTRY_PREFIX      Prx

// Undefine the __purecall provided by rpcproxy.h so that it does not conflict
// with the libc definition.
#include <rpcproxy.h>
#ifdef DLLDUMMYPURECALL
#undef DLLDUMMYPURECALL
#define DLLDUMMYPURECALL
#endif

#include "goopdate/google_update_idl_data.c"
#include "goopdate/google_update_idl_p.c"

#pragma warning(pop)

