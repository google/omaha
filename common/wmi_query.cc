// Copyright 2006-2009 Google Inc.
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


#include "omaha/common/wmi_query.h"

#include <atlcomcli.h>
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"

namespace omaha {

WmiQuery::WmiQuery() : at_end_(true) {
}

WmiQuery::~WmiQuery() {
}

HRESULT WmiQuery::Connect(const TCHAR* resource) {
  UTIL_LOG(L6, (_T("[WmiQuery::Connect][resource=%s]"), resource));
  ASSERT1(resource && *resource);

  CComBSTR object_path;
  HRESULT hr = object_path.Append(resource);
  if (FAILED(hr)) {
    return hr;
  }

  hr = wbem_.CoCreateInstance(__uuidof(WbemLocator));
  if (FAILED(hr)) {
    return hr;
  }

  // Connect to WMI through the IWbemLocator::ConnectServer method. This
  // call can block up to 2 minutes on XP or indefinitely on Windows 2000 if
  // the server is broken.
  hr = wbem_->ConnectServer(object_path,                      // object path
                            NULL,                             // username
                            NULL,                             // password
                            NULL,                             // locale
                            WBEM_FLAG_CONNECT_USE_MAX_WAIT,   // security flags
                            NULL,                             // authority
                            0,                                // context
                            &service_);                       // namespace
  if (FAILED(hr)) {
    return hr;
  }

  // Set security levels on the proxy.
  hr = ::CoSetProxyBlanket(service_,
                           RPC_C_AUTHN_WINNT,
                           RPC_C_AUTHZ_NONE,
                           NULL,
                           RPC_C_AUTHN_LEVEL_CALL,
                           RPC_C_IMP_LEVEL_IMPERSONATE,
                           NULL,
                           EOAC_NONE);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT WmiQuery::Query(const TCHAR* query) {
  UTIL_LOG(L6, (_T("[WmiQuery::Query][query=%s]"), query));
  ASSERT1(query && *query);

  CComBSTR query_language, query_string;
  HRESULT hr = query_language.Append(_T("WQL"));
  if (FAILED(hr)) {
    return hr;
  }
  hr = query_string.Append(query);
  if (FAILED(hr)) {
    return hr;
  }

  uint32 flags = WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY;
  hr = service_->ExecQuery(query_language,
                           query_string,
                           flags,
                           NULL,
                           &enumerator_);
  if (FAILED(hr)) {
    return hr;
  }
  at_end_ = false;
  return Next();
}

HRESULT WmiQuery::Next() {
  UTIL_LOG(L6, (_T("[WmiQuery::Next]")));

  ASSERT1(!at_end_);

  ULONG ret = 0;
  HRESULT hr = enumerator_->Next(WBEM_INFINITE, 1, &obj_, &ret);
  if (FAILED(hr)) {
    return hr;
  }
  at_end_ = ret == 0;
  return S_OK;
}

bool WmiQuery::AtEnd() {
  return at_end_;
}

HRESULT WmiQuery::GetValue(const TCHAR* name, CComVariant* value) {
  ASSERT1(name && *name);
  ASSERT1(value);
  ASSERT1(!at_end_ && obj_);

  value->Clear();

  CComBSTR name_string;
  HRESULT hr = name_string.Append(name);
  if (FAILED(hr)) {
    return hr;
  }
  hr = obj_->Get(name_string, 0, value, 0, 0);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT WmiQuery::GetValue(const TCHAR* name, CString* value) {
  ASSERT1(name && *name);
  ASSERT1(value);

  CComVariant var;
  HRESULT hr = GetValue(name, &var);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(V_VT(&var) == VT_BSTR);
  value->SetString(var.bstrVal);
  return S_OK;
}

HRESULT WmiQuery::GetValue(const TCHAR* name, bool* value) {
  ASSERT1(name && *name);
  ASSERT1(value);

  CComVariant var;
  HRESULT hr = GetValue(name, &var);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(V_VT(&var) == VT_BOOL);
  *value = var.boolVal != 0;
  return S_OK;
}

HRESULT WmiQuery::GetValue(const TCHAR* name, int* value) {
  ASSERT1(name && *name);
  ASSERT1(value);

  CComVariant var;
  HRESULT hr = GetValue(name, &var);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(V_VT(&var) == VT_I4);
  *value = var.lVal;
  return S_OK;
}

HRESULT WmiQuery::GetValue(const TCHAR* name, uint32* value) {
  ASSERT1(name && *name);
  ASSERT1(value);

  CComVariant var;
  HRESULT hr = GetValue(name, &var);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(V_VT(&var) == VT_UI4);
  *value = var.ulVal;
  return S_OK;
}

}  // namespace omaha

