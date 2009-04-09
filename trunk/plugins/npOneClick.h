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
// The OneClick NPAPI Plugin implementation
// using the new NPRuntime supported by Firefox and other

#ifndef OMAHA_PLUGINS_NPONECLICK_H__
#define OMAHA_PLUGINS_NPONECLICK_H__

#include "omaha/third_party/gecko/include/npupp.h"

#include <atlsafe.h>              // NOLINT

#include "omaha/plugins/baseplugin.h"

// These files need to be included _after_ the headers above because
// npupp.h includes windows.h in a special way and defines uint32 and so
// forth on its own.  If we include these "standard" headers first, then
// the type system gets all confused.  However, including these afterwards
// makes lint angry since you're not normally supposed to do this.  But that's
// why there is the NOLINT comment after each of the header lines below.
#include <ATLBase.h>              // NOLINT
#include <ATLSafe.h>              // NOLINT
#include <ATLCom.h>               // NOLINT
#include <ATLStr.h>               // NOLINT
#include <ATLConv.h>              // NOLINT
#include <ObjSafe.h>              // NOLINT

// Need to do the following, rather than #include common/basictypes.h
// because some defines/types in basictypes.h conflict with the NPAPI
// headers

// Force basictypes.h to not be included
#define BASE_BASICTYPES_H_

// #undef ATLASSERT, we will use the one from atlassert.h #included below
#undef ATLASSERT

// Copied from basictypes.h
// Typedefing some values that are needed within logging.h
typedef uint64 time64;

// Copied from basictypes.h
// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_EVIL_CONSTRUCTORS(TypeName)    \
  TypeName(const TypeName&);                    \
  TypeName &operator=(const TypeName&)

#include "common/atlconvfix.h"
// logging.h contains definitions for PLUGIN_LOG
#include "common/logging.h"
#include "common/atlassert.h"
// debug.h contains definitions for ASSERT
#include "common/debug.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(*(x)))
#endif

// Helper macros
// TRACE, ASSERT and return if the first argument evaluates to false
#define RETTRACE_IF_FALSE(x)                                \
  do {                                                      \
    BOOL success(!!(x));                                    \
    if (!success) {                                         \
      PLUGIN_LOG(L2, (_T("[") _T(#x) _T("] is FALSE")));    \
      ASSERT(FALSE, (_T("[") _T(#x) _T("] is FALSE")));     \
      return;                                               \
    }                                                       \
  } while (false)                                           \

// TRACE, ASSERT and return if the HRESULT argument is a COM error
#define RETTRACE_IF_FAILED(x)  RETTRACE_IF_FALSE(SUCCEEDED(x))

// TRACE, throw exception and return if the first argument evaluates to false
#define RETTRACE_EXCEPTION_IF_FALSE(x)                      \
  do {                                                      \
    BOOL success(!!(x));                                    \
    if (!success) {                                         \
      PLUGIN_LOG(L2, (_T("[") _T(#x) _T("] is FALSE")));    \
      ASSERT(FALSE, (_T("[") _T(#x) _T("] is FALSE")));     \
      NPN_SetException(this, "false");                      \
      return true;                                          \
    }                                                       \
  } while (false)                                           \

// TRACE, throw exception and return if the HRESULT argument is a COM error
#define RETTRACE_EXCEPTION_IF_FAILED(x)                     \
  do {                                                      \
    HRESULT hr = (x);                                       \
    if (FAILED(hr)) {                                       \
      PLUGIN_LOG(L2, (_T("[") _T(#x) _T("] is FAILED")));   \
      ASSERT(FALSE, (_T("[") _T(#x) _T("] is FAILED")));    \
      char buf[32];                                         \
      ::wnsprintfA(buf, ARRAYSIZE(buf), "0x%08x", hr);      \
      NPN_SetException(this, buf);                          \
      return true;                                          \
    }                                                       \
  } while (false)                                           \

// Simple helper macro for the class to define bool based properties
#define DEFINE_BOOL_PROPERTY(the_class, x, instance, method)                  \
bool the_class::Get##x(NPVariant *result) {                                   \
  PLUGIN_LOG(L2, (_T("[") _T(#the_class) _T("::Get") _T(#x) _T("]")));        \
  RETTRACE_EXCEPTION_IF_FALSE(instance != NULL);                              \
  VARIANT_BOOL yes = VARIANT_FALSE;                                           \
  RETTRACE_EXCEPTION_IF_FAILED(instance->method(&yes));                       \
  BOOLEAN_TO_NPVARIANT(yes == VARIANT_TRUE, *result);                         \
  return true;                                                                \
}                                                                             \
                                                                              \
bool the_class::Set##x(const NPVariant *value) {                              \
  PLUGIN_LOG(L2, (_T("[") _T(#the_class) _T("::Set") _T(#x)                   \
                  _T("] - Not Implemented")));                                \
  /* Script should not be calling this! */                                    \
  ATLASSERT(FALSE);                                                           \
  return false;                                                               \
}

// Simple helper macro for the class to define string based properties
#define DEFINE_STRING_PROPERTY(the_class, x, instance, method)                \
bool the_class::Get##x(NPVariant *result) {                                   \
  PLUGIN_LOG(L2, (_T("[") _T(#the_class) _T("::Get") _T(#x) _T("]")));        \
  RETTRACE_EXCEPTION_IF_FALSE(instance);                                      \
  CComBSTR bstr;                                                              \
  instance->method(&bstr);                                                    \
  size_t cch = bstr.Length() + 1;                                             \
  RETTRACE_EXCEPTION_IF_FALSE(cch);                                           \
  /* Use NPN_MemAlloc() to allocate [out] parameter memory */                 \
  char* result_##x = reinterpret_cast<char *>(NPN_MemAlloc(cch));             \
  RETTRACE_EXCEPTION_IF_FALSE(::lstrcpynA(result_##x, CW2A(bstr), cch));      \
  PLUGIN_LOG(L2, (_T("[") _T(#the_class) _T("::Get") _T(#x)                   \
                  _T("][retval=%S]"), result_##x));                           \
  STRINGZ_TO_NPVARIANT(result_##x, *result);                                  \
  return true;                                                                \
}                                                                             \
                                                                              \
bool the_class::Set##x(const NPVariant *value) {                              \
  PLUGIN_LOG(L2, (_T("[") _T(#the_class) _T("::Set") _T(#x)                   \
                  _T("] - Not Implemented")));                                \
  /* Script should not be calling this! */                                    \
  ATLASSERT(FALSE);                                                           \
  return false;                                                               \
}                                                                             \

namespace omaha {

class OneClickWorker;

}  // namespace omaha


// The primary plug-in class, NPOneClickClass.
DECLARE_PLUGIN_CLASS(NPOneClickClass)
  virtual void Shutdown();
  DECLARE_PLUGIN_FUNCTION(Install)
  DECLARE_PLUGIN_FUNCTION(Install2)
  DECLARE_PLUGIN_FUNCTION(GetInstalledVersion)
  DECLARE_PLUGIN_FUNCTION(GetOneClickVersion)
  DECLARE_CPLUGIN_NPCLASS()
 private:
  HRESULT EnsureWorkerUrlSet();
  HRESULT GetUrl(CString* url_str);

  scoped_ptr<omaha::OneClickWorker> oneclick_worker_;
  bool is_worker_url_set_;
END_DECLARE_PLUGIN_CLASS(NPOneClickClass)

#endif  // OMAHA_PLUGINS_NPONECLICK_H__

