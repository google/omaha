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
// using the new NPRuntime supported by Firefox and others
//
//

#pragma warning(push)
#pragma warning(disable : 4201 4265)
// 4201: nonstandard extension used : nameless struct/union
// 4265: class has virtual functions, but destructor is not virtual

#include "base/scoped_ptr.h"
#include "omaha/plugins/npOneClick.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/string.h"
#include "omaha/plugins/oneclick_browser_callback_npapi.h"
#include "omaha/plugins/oneclick_worker.h"

#pragma warning(pop)

using omaha::ScopeGuard;
using omaha::MakeGuard;

// Convert NPString to a const wide string.
CString ConvertNPStringToString(const NPString utf8_str) {
  CString utf16_str;
  if (!utf8_str.utf8characters || !utf8_str.utf8length) {
    return utf16_str;
  }

  int len = ::MultiByteToWideChar(CP_UTF8,
                                  0,
                                  utf8_str.utf8characters,
                                  utf8_str.utf8length,
                                  NULL,
                                  0);
  ATLASSERT(len);
  if (!len) {
    return utf16_str;
  }
  ATLVERIFY(::MultiByteToWideChar(CP_UTF8,
                                  0,
                                  utf8_str.utf8characters,
                                  utf8_str.utf8length,
                                  CStrBuf(utf16_str, len, CStrBuf::SET_LENGTH),
                                  len));

  PLUGIN_LOG(L2, (_T("[ConvertNPStringToString][%s]"), utf16_str));
  return utf16_str;
}

// The primary plug-in class, NPOneClickClass.
BEGIN_PLUGIN_CLASS(NPOneClickClass)
  PLUGIN_FUNCTION(NPOneClickClass, Install, 3)
  PLUGIN_FUNCTION(NPOneClickClass, Install2, 1)
  PLUGIN_FUNCTION(NPOneClickClass, GetInstalledVersion, 2)
  PLUGIN_FUNCTION(NPOneClickClass, GetOneClickVersion, 0)
END_BEGIN_PLUGIN_CLASS

DEFINE_CPLUGIN_NPCLASS(NPOneClickClass)
  PLUGIN_LOG(L2, (_T("[NPOneClickClass::Static constructor]")));
  return TRUE;
END_DEFINE_CPLUGIN_NPCLASS()

BEGIN_PLUGIN_CLASS_CTOR(NPOneClickClass)
  PLUGIN_LOG(L2, (_T("[NPOneClickClass::constructor]")));

  is_worker_url_set_ = false;
  oneclick_worker_.reset(new omaha::OneClickWorker());
  RETTRACE_IF_FAILED(oneclick_worker_->Initialize());
END_PLUGIN_CLASS_CTOR

BEGIN_PLUGIN_CLASS_DTOR(NPOneClickClass)
  PLUGIN_LOG(L2, (_T("[NPOneClickClass::destructor]")));
END_PLUGIN_CLASS_DTOR

void NPOneClickClass::Shutdown() {
  VERIFY1(SUCCEEDED(oneclick_worker_->Shutdown()));
}

HRESULT NPOneClickClass::EnsureWorkerUrlSet() {
  if (is_worker_url_set_) {
    return S_OK;
  }

  CString browser_url_str;
  HRESULT hr = GetUrl(&browser_url_str);
  if (FAILED(hr)) {
    PLUGIN_LOG(LE, (_T("[GetUrl failed][0x%x]"), hr));
    return hr;
  }

  oneclick_worker_->set_browser_url(browser_url_str);
  is_worker_url_set_ = true;
  return S_OK;
}

// STDMETHOD(Install)(BSTR cmd_line_args,
//                    VARIANT* success_callback,
//                    VARIANT* failure_callback);
bool NPOneClickClass::Install(const NPVariant* args,
                              uint32_t argCount,
                              NPVariant* result) {
  UNREFERENCED_PARAMETER(result);

  PLUGIN_LOG(L2, (_T("[NPOneClickClass::Install]")));
  RETTRACE_EXCEPTION_IF_FAILED(EnsureWorkerUrlSet());

  RETTRACE_EXCEPTION_IF_FALSE(argCount == 3 &&
                              args &&
                              NPVARIANT_IS_STRING(args[0]));

  omaha::OneClickBrowserCallbackNpapi browser_callback;
  RETTRACE_EXCEPTION_IF_FAILED(browser_callback.Initialize(npp_,
                                                           args[1],
                                                           args[2]));

  RETTRACE_EXCEPTION_IF_FAILED(oneclick_worker_->DoOneClickInstall(
      ConvertNPStringToString(NPVARIANT_TO_STRING(args[0])),
      &browser_callback));

  return true;
}

// STDMETHOD(Install2)(BSTR extra_args);
bool NPOneClickClass::Install2(const NPVariant* args,
                               uint32_t argCount,
                               NPVariant* result) {
  UNREFERENCED_PARAMETER(result);

  PLUGIN_LOG(L2, (_T("[NPOneClickClass::Install2]")));
  RETTRACE_EXCEPTION_IF_FAILED(EnsureWorkerUrlSet());

  RETTRACE_EXCEPTION_IF_FALSE(argCount == 1 &&
                              args &&
                              NPVARIANT_IS_STRING(args[0]));

  RETTRACE_EXCEPTION_IF_FAILED(oneclick_worker_->DoOneClickInstall2(
      ConvertNPStringToString(NPVARIANT_TO_STRING(args[0]))));

  return true;
}

// STDMETHOD(GetInstalledVersion)(BSTR guid_string,
//                                VARIANT_BOOL is_machine,
//                                BSTR* version_string);
bool NPOneClickClass::GetInstalledVersion(const NPVariant* args,
                                          uint32_t argCount,
                                          NPVariant* result) {
  PLUGIN_LOG(L2, (_T("[NPOneClickClass::GetInstalledVersion]")));
  RETTRACE_EXCEPTION_IF_FAILED(EnsureWorkerUrlSet());

  NULL_TO_NPVARIANT(*result);
  RETTRACE_EXCEPTION_IF_FALSE(argCount == 2 &&
                              args &&
                              NPVARIANT_IS_STRING(args[0]) &&
                              NPVARIANT_IS_BOOLEAN(args[1]));

  CString version;
  RETTRACE_EXCEPTION_IF_FAILED(oneclick_worker_->GetInstalledVersion(
      ConvertNPStringToString(NPVARIANT_TO_STRING(args[0])),
      NPVARIANT_TO_BOOLEAN(args[1]),
      &version));

  size_t version_length = version.GetLength() + 1;
  RETTRACE_EXCEPTION_IF_FALSE(version_length);
  char* version_out = reinterpret_cast<char *>(NPN_MemAlloc(version_length));
  RETTRACE_EXCEPTION_IF_FALSE(version_out);
  RETTRACE_EXCEPTION_IF_FALSE(
      ::lstrcpynA(version_out, CStringA(version), version_length));

  PLUGIN_LOG(L2, (_T("[GetInstalledVersion][%S]"), version_out));
  STRINGZ_TO_NPVARIANT(version_out, *result);
  return true;
}

// STDMETHOD(GetOneClickVersion)(long* version);
bool NPOneClickClass::GetOneClickVersion(const NPVariant* args,
                                         uint32_t argCount,
                                         NPVariant* result) {
  UNREFERENCED_PARAMETER(args);
  PLUGIN_LOG(L2, (_T("[NPOneClickClass::GetOneClickVersion]")));
  RETTRACE_EXCEPTION_IF_FAILED(EnsureWorkerUrlSet());

  NULL_TO_NPVARIANT(*result);
  RETTRACE_EXCEPTION_IF_FALSE(argCount == 0);

  int32 version = 0;
  RETTRACE_EXCEPTION_IF_FAILED(oneclick_worker_->GetOneClickVersion(&version));

  INT32_TO_NPVARIANT(version, *result);
  return true;
}

// get the URL that we are currently hosted in
// Essentially, we return window.location.href
HRESULT NPOneClickClass::GetUrl(CString* url_str) {
  PLUGIN_LOG(L2, (_T("[NPOneClickClass::GetUrl]")));
  // If npp_ is NULL, Init() has not been called
  ATLASSERT(npp_);
  ATLASSERT(url_str != NULL);
  if (npp_ == NULL || url_str == NULL) {
    return E_POINTER;
  }

  NPObject* window_object = NULL;

  // Reference count not bumped up, do not release window_object
  NPN_GetValue(npp_, NPNVWindowNPObject, &window_object);
  if (!window_object)  {
    return E_UNEXPECTED;
  }

  NPIdentifier location_id = NPN_GetStringIdentifier("location");
  NPIdentifier href_id = NPN_GetStringIdentifier("href");

  NPVariant locationv;
  // Initialize the variant to NULL
  NULL_TO_NPVARIANT(locationv);
  NPN_GetProperty(npp_, window_object, location_id, &locationv);
  ON_SCOPE_EXIT(NPN_ReleaseVariantValue, &locationv);

  NPObject* location = NULL;
  if (NPVARIANT_IS_OBJECT(locationv)) {
    location = NPVARIANT_TO_OBJECT(locationv);
  }

  if (!location) {
    return E_UNEXPECTED;
  }

  NPVariant hrefv;
  // Initialize the variant to NULL
  NULL_TO_NPVARIANT(hrefv);
  NPN_GetProperty(npp_, location, href_id, &hrefv);
  ON_SCOPE_EXIT(NPN_ReleaseVariantValue, &hrefv);

  if (NPVARIANT_IS_STRING(hrefv)) {
    CString url(ConvertNPStringToString(NPVARIANT_TO_STRING(hrefv)));
    ATLASSERT(!url.IsEmpty());
    if (!url.IsEmpty()) {
      *url_str = url;
    }
  }

  return S_OK;
}

