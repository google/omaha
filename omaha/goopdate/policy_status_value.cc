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

#include "omaha/goopdate/policy_status_value.h"
#include <atlsafe.h>
#include <stdint.h>
#include <limits>
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"

namespace omaha {

HRESULT PolicyStatusValue::Create(
    const CString& source,
    const CString& value,
    bool has_conflict,
    const CString& conflict_source,
    const CString& conflict_value,
    IPolicyStatusValue** policy_status_value) {
  ASSERT1(policy_status_value);

  CComObject<PolicyStatusValue>* v = NULL;
  HRESULT hr = CComObject<PolicyStatusValue>::CreateInstance(&v);
  if (FAILED(hr)) {
    return hr;
  }

  v->source_ = source.AllocSysString();
  v->value_ = value.AllocSysString();
  v->has_conflict_ = has_conflict ? VARIANT_TRUE : VARIANT_FALSE;
  v->conflict_source_ = conflict_source.AllocSysString();
  v->conflict_value_ = conflict_value.AllocSysString();

  CComPtr<IPolicyStatusValue> status_value(v);
  *policy_status_value = status_value.Detach();

  return S_OK;
}

PolicyStatusValue::PolicyStatusValue() : m_bRequiresSave(TRUE),
                                         has_conflict_(VARIANT_FALSE) {
}

PolicyStatusValue::~PolicyStatusValue() {
}

// IPolicyStatusValue.
STDMETHODIMP PolicyStatusValue::get_source(BSTR* source) {
  ASSERT1(source);

  *source = source_.Copy();
  return S_OK;
}

STDMETHODIMP PolicyStatusValue::get_value(BSTR* value) {
  ASSERT1(value);

  *value = value_.Copy();
  return S_OK;
}

STDMETHODIMP PolicyStatusValue::get_hasConflict(VARIANT_BOOL* has_conflict) {
  ASSERT1(has_conflict);

  *has_conflict = has_conflict_;
  return S_OK;
}

STDMETHODIMP PolicyStatusValue::get_conflictSource(BSTR* conflict_source) {
  ASSERT1(conflict_source);

  *conflict_source = conflict_source_.Copy();
  return S_OK;
}

STDMETHODIMP PolicyStatusValue::get_conflictValue(BSTR* conflict_value) {
  ASSERT1(conflict_value);

  *conflict_value = conflict_value_.Copy();
  return S_OK;
}

}  // namespace omaha
