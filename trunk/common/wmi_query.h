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

#ifndef OMAHA_COMMON_WMI_QUERY_H_
#define OMAHA_COMMON_WMI_QUERY_H_

#include <windows.h>
#include <wbemidl.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

class WmiQuery {
 public:
  WmiQuery();
  ~WmiQuery();

  // Connects to the server to get WMI service.
  HRESULT Connect(const TCHAR* resource);

  // Queries the service.
  HRESULT Query(const TCHAR* query);

  // Reads the next row.
  HRESULT Next();

  // Returns true at the end.
  bool AtEnd();

  // Gets the value of the named property.
  HRESULT GetValue(const TCHAR* name, CComVariant* value);
  HRESULT GetValue(const TCHAR* name, CString* value);
  HRESULT GetValue(const TCHAR* name, bool* value);
  HRESULT GetValue(const TCHAR* name, int* value);
  HRESULT GetValue(const TCHAR* name, uint32* value);

 private:
  CComPtr<IWbemLocator> wbem_;
  CComPtr<IWbemServices> service_;
  CComPtr<IEnumWbemClassObject> enumerator_;
  CComPtr<IWbemClassObject> obj_;
  bool at_end_;

  DISALLOW_EVIL_CONSTRUCTORS(WmiQuery);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_WMI_QUERY_H_

