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

// PolicyStatusValue represents the managed state of a single Google Update
// policy. It contains the current source and value, as well as if any conflicts
// exist with that policy.
// IPolicyStatusValue can be queried for the current source and value, as well
// as if any conflicts exist with that policy.
// PolicyStatusValue implements IMarshal to marshal itself by value for
// performance, since this reduces the number of interprocess calls.

#ifndef OMAHA_GOOPDATE_POLICY_STATUS_VALUE_H_
#define OMAHA_GOOPDATE_POLICY_STATUS_VALUE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <vector>

#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/constants.h"
#include "omaha/base/marshal_by_value.h"
#include "omaha/goopdate/google_update_ps_resource.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

class ATL_NO_VTABLE PolicyStatusValue
  : public CComObjectRootEx<CComObjectThreadModel>,
    public CComCoClass<PolicyStatusValue>,
    public IDispatchImpl<IPolicyStatusValue,
                         &__uuidof(IPolicyStatusValue),
                         &CAtlModule::m_libid,
                         kMajorTypeLibVersion,
                         kMinorTypeLibVersion>,
    public IPersistStreamInitImpl<PolicyStatusValue>,
    public MarshalByValue<PolicyStatusValue> {
 public:
  static HRESULT Create(const CString& source,
                        const CString& value,
                        bool has_conflict,
                        const CString& conflict_source,
                        const CString& conflict_value,
                        IPolicyStatusValue** policy_status_value);
  PolicyStatusValue();

  static bool is_machine() {
    return goopdate_utils::IsRunningFromOfficialGoopdateDir(true);
  }

  static const CLSID& GetObjectCLSID() {
    return is_machine() ? __uuidof(PolicyStatusValueMachineClass) :
                          __uuidof(PolicyStatusValueUserClass);
  }

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_INPROC_SERVER_RGS)

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"), is_machine() ? _T("HKLM") : _T("HKCU"))
    REGMAP_ENTRY(_T("CLSID"), GetObjectCLSID())
  END_REGISTRY_MAP()

  BEGIN_PROP_MAP(PolicyStatusValue)
    PROP_DATA_ENTRY("Source", source_, VT_BSTR)
    PROP_DATA_ENTRY("Value", value_, VT_BSTR)
    PROP_DATA_ENTRY("HasConflict", has_conflict_, VT_BOOL)
    PROP_DATA_ENTRY("ConflictSource", conflict_source_, VT_BSTR)
    PROP_DATA_ENTRY("ConflictValue", conflict_value_, VT_BSTR)
  END_PROP_MAP()

  // IPolicyStatusValue.
  STDMETHOD(get_source)(BSTR* source);
  STDMETHOD(get_value)(BSTR* value);
  STDMETHOD(get_hasConflict)(VARIANT_BOOL* has_conflict);
  STDMETHOD(get_conflictSource)(BSTR* conflict_source);
  STDMETHOD(get_conflictValue)(BSTR* conflict_value);

 protected:
  virtual ~PolicyStatusValue();

  BEGIN_COM_MAP(PolicyStatusValue)
    COM_INTERFACE_ENTRY(IPolicyStatusValue)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IMarshal)
  END_COM_MAP()

  BOOL m_bRequiresSave;

 private:
  CComBSTR source_;
  CComBSTR value_;
  VARIANT_BOOL has_conflict_;
  CComBSTR conflict_source_;
  CComBSTR conflict_value_;

  DISALLOW_COPY_AND_ASSIGN(PolicyStatusValue);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_POLICY_STATUS_VALUE_H_

