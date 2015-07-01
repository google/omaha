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

#include "omaha/plugins/update/npapi/testing/dispatch_host_test_interface.h"

namespace omaha {

STDMETHODIMP DispatchHostTestInterface::Random(INT* x) {
  *x = 42;
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface::get_Property(INT* x) {
  *x = 0xdeadbeef;
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface::put_Property(INT x) {
  UNREFERENCED_PARAMETER(x);
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface::get_ReadOnlyProperty(INT* x) {
  *x = 19700101;
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface::put_WriteOnlyProperty(INT x) {
  UNREFERENCED_PARAMETER(x);
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface::AddAsMethod(INT a, INT b, INT* c) {
  *c = a + b;
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface::get_AddAsProperty(
    INT a, INT b, INT* c) {
  *c = a + b;
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface::DidYouMeanRecursion(IDispatch** me) {
  *me = this;
  return S_OK;
}

STDMETHODIMP DispatchHostTestInterface2::get_Get(INT index, INT* x) {
  *x = index << 1;
  return S_OK;
}

}  // namespace omaha
