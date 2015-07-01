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

// This class allows for implementing two dual interfaces, ReadOnly being a
// read-only dual interface, and ReadWrite being a read-write dual interface.
// Based on the value returned from is_read_only(), the ReadOnly or the
// ReadWrite dual interface is exposed at runtime.
//
// An ATL class needs to
// (a) derive from DualInterfaceSwitch
//
// (b) implement the is_read_only() method. For instance:
//   bool is_read_only() { return is_read_only_; }
//
// (c) add entries similar to below for the dual interfaces as well as IDispatch
//     to the COM map:
//  BEGIN_COM_MAP(AppData)
//    COM_INTERFACE_ENTRY2(IUnknown, IAppData)
//    COM_INTERFACE_ENTRY_FUNC(__uuidof(IAppDataReadOnly), 0, DualInterfaceQI)
//    COM_INTERFACE_ENTRY_FUNC(__uuidof(IAppData), 0, DualInterfaceQI)
//    COM_INTERFACE_ENTRY_FUNC(__uuidof(IDispatch), 0, DualInterfaceQI)
//  END_COM_MAP()

#ifndef OMAHA_BASE_DUAL_INTERFACE_SWITCH_H_
#define OMAHA_BASE_DUAL_INTERFACE_SWITCH_H_

#include <windows.h>
#include <atlbase.h>
#include "base/basictypes.h"
#include "omaha/base/debug.h"

namespace omaha {

template <class T, class ReadOnly, REFIID iid_read_only,
          class ReadWrite, REFIID iid_read_write>
class ATL_NO_VTABLE DualInterfaceSwitch : public ReadOnly,
                                          public ReadWrite {
 protected:
  // DualInterfaceQI gives out either the ReadOnly or ReadWrite interface
  // pointer, based on the value of is_read_only() that class T implements.
  // The signature of this function has to be
  //   HRESULT WINAPI func(void* pv, REFIID riid, LPVOID* ppv, DWORD dw);
  // for it to work with COM_INTERFACE_ENTRY_FUNC.
  static HRESULT WINAPI DualInterfaceQI(void* p,
                                        REFIID riid,
                                        LPVOID* v,
                                        DWORD) {
    // Ensure that ReadOnly and ReadWrite are derived from IDispatch.
    ASSERT1(static_cast<IDispatch*>(reinterpret_cast<ReadOnly*>(1)));
    ASSERT1(static_cast<IDispatch*>(reinterpret_cast<ReadWrite*>(1)));

    if (riid != iid_read_only &&
        riid != iid_read_write &&
        riid != __uuidof(IDispatch)) {
      // Returning S_FALSE allows COM map processing to continue. Another entry
      // in the COM map might match the interface requested.
      return S_FALSE;
    }

    T* t = reinterpret_cast<T*>(p);
    if (t->is_read_only() && riid == iid_read_write) {
      return S_FALSE;
    }

    t->AddRef();
    if (t->is_read_only() || riid == iid_read_only) {
      *v = static_cast<ReadOnly*>(t);
      return S_OK;
    }

    *v = static_cast<ReadWrite*>(t);
    return S_OK;
  }
};

}  // namespace omaha

#endif  // OMAHA_BASE_DUAL_INTERFACE_SWITCH_H_
