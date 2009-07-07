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

#include "omaha/common/firewall_product_detection.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/utils.h"
#include "omaha/common/wmi_query.h"

namespace omaha {

namespace firewall_detection {

namespace {

const TCHAR kWmiSecurityCenter[]        = _T("root\\SecurityCenter");
const TCHAR kWmiQueryFirewallProduct[]  = _T("select * from FirewallProduct");
const TCHAR kWmiPropDisplayName[]       = _T("displayName");
const TCHAR kWmiPropVersionNumber[]     = _T("versionNumber");

}  // namespace


HRESULT Detect(CString* name, CString* version) {
  ASSERT1(name);
  ASSERT1(version);

  name->Empty();
  version->Empty();

  WmiQuery wmi_query;
  HRESULT hr = wmi_query.Connect(kWmiSecurityCenter);
  if FAILED(hr) {
    return hr;
  }
  hr = wmi_query.Query(kWmiQueryFirewallProduct);
  if (FAILED(hr)) {
    return hr;
  }
  if (wmi_query.AtEnd()) {
    return E_FAIL;
  }
  hr = wmi_query.GetValue(kWmiPropDisplayName, name);
  if (FAILED(hr)) {
    return hr;
  }
  wmi_query.GetValue(kWmiPropVersionNumber, version);
  return S_OK;
}

}  // namespace firewall_detection

}  // namespace omaha

