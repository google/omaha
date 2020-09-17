// Copyright 2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_BROKER_CLASS_FACTORY_H_
#define OMAHA_GOOPDATE_BROKER_CLASS_FACTORY_H_

#include <atlcom.h>
#include "base/basictypes.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/system.h"
#include "omaha/base/vistautil.h"
#include "omaha/goopdate/elevation_moniker_resource.h"

namespace omaha {

// This class factory delegates CoCreation to the template "clsid" and returns
// the resulting interface to the caller. If unable to CoCreate "clsid",
// BrokerClassFactory attempts to CoCreateAsAdmin and return "clsid2" to the
// caller.
template <const CLSID& clsid, const CLSID& clsid2>
class BrokerClassFactory : public CComClassFactory {
 public:
  BrokerClassFactory() {}

  virtual ~BrokerClassFactory() {}

  STDMETHOD(CreateInstance)(LPUNKNOWN outer_unk, REFIID riid, void** instance) {
    CORE_LOG(L3, (_T("[BrokerClassFactory CreateInstance][%s]"),
                  GuidToString(riid)));

    // The LockServer combo is used to pulse the module count, which will
    // shutdown the server after this CreateInstance() request completes,
    // provided there are no other outstanding interface references being held
    // by clients.
    LockServer(TRUE);
    ON_SCOPE_EXIT_OBJ(*this, &IClassFactory::LockServer, FALSE);

    if (!instance) {
      return E_POINTER;
    }

    *instance = NULL;

    if (outer_unk) {
      return CLASS_E_NOAGGREGATION;
    }

    HRESULT hr = ::CoCreateInstance(clsid,
                                    outer_unk,
                                    CLSCTX_ALL,
                                    riid,
                                    instance);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[Create failed][%s][0x%x]"), GuidToString(clsid), hr));

      if (!vista_util::IsVistaOrLater() && !vista_util::IsUserAdmin()) {
        return hr;
      }

      hr = System::CoCreateInstanceAsAdmin(NULL, clsid2, riid, instance);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[Create fail][%s][0x%x]"), GuidToString(clsid2), hr));
        return hr;
      }
    }

    return S_OK;
  }
};


// This class is used for COM registration and class factory registration and
// instantiation of the delegate brokers. The class itself is not
// instantiated. target_clsid is the CLSID that BrokerClassFactory
// CoCreates and returns to the caller. If unable to CoCreate target_clsid,
// BrokerClassFactory attempts to CoCreateAsAdmin target_clsid2. broker_clsid is
// the CLSID that clients of the broker CoCreate.
template <const CLSID& target_clsid, const CLSID& target_clsid2,
          const CLSID& broker_clsid, const TCHAR* const broker_progid>
class ATL_NO_VTABLE BrokerClassFactoryRegistrar
    : public CComObjectRootEx<CComObjectThreadModel>,
      public CComCoClass<BrokerClassFactoryRegistrar<target_clsid,
                                                     target_clsid2,
                                                     broker_clsid,
                                                     broker_progid> > {
 public:
  BrokerClassFactoryRegistrar() {
    ASSERT1(false);
  }

  typedef BrokerClassFactory<target_clsid, target_clsid2> BrokerClassFactoryT;

  DECLARE_CLASSFACTORY_EX(BrokerClassFactoryT);
  DECLARE_NOT_AGGREGATABLE(BrokerClassFactoryRegistrar);
  DECLARE_REGISTRY_RESOURCEID_EX(IDR_LOCAL_SERVER_ELEVATION_RGS)

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("PROGID"), broker_progid)
    REGMAP_ENTRY(_T("VERSION"), _T("1.0"))
    REGMAP_ENTRY(L"DESCRIPTION", L"Google Update Broker Class Factory")
    REGMAP_ENTRY(L"CLSID", broker_clsid)
    REGMAP_ENTRY(L"ICONRESID", PP_STRINGIZE(IDI_ELEVATION_MONIKER_ICON))
    REGMAP_ENTRY(L"STRINGRESID",
                 PP_STRINGIZE(IDS_ELEVATION_MONIKER_DISPLAYNAME))
    REGMAP_MODULE2(L"MODULE", kOmahaBrokerFileName)
  END_REGISTRY_MAP()

  BEGIN_COM_MAP(BrokerClassFactoryRegistrar)
  END_COM_MAP()

 protected:
  virtual ~BrokerClassFactoryRegistrar() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrokerClassFactoryRegistrar);
};


extern TCHAR kOnDemandMachineBrokerProgId[];
extern TCHAR kUpdate3WebMachineBrokerProgId[];
extern TCHAR kPolicyStatusMachineBrokerProgId[];

// An OnDemand client CoCreates OnDemandMachineAppsClass, which
// instantiates the class factory for the OnDemandMachineBroker typedef below.
// The class factory in turn passes the CreateInstance through to
// OnDemandMachineAppsServiceClass.
typedef BrokerClassFactoryRegistrar<__uuidof(OnDemandMachineAppsServiceClass),
                                    __uuidof(OnDemandMachineAppsFallbackClass),
                                    __uuidof(OnDemandMachineAppsClass),
                                    kOnDemandMachineBrokerProgId>
                                    OnDemandMachineBroker;

// The Pack web plugin client CoCreates GoogleUpdate3WebMachineClass, which
// instantiates the class factory for the Update3WebBroker typedef below. The
// class factory in turn passes the CreateInstance through to
// GoogleUpdate3WebServiceClass.
typedef BrokerClassFactoryRegistrar<
    __uuidof(GoogleUpdate3WebServiceClass),
    __uuidof(GoogleUpdate3WebMachineFallbackClass),
    __uuidof(GoogleUpdate3WebMachineClass),
    kUpdate3WebMachineBrokerProgId>
    Update3WebMachineBroker;

// A Policy Status client CoCreates PolicyStatusMachineClass, which
// instantiates the class factory for the PolicyStatusMachineBroker typedef
// below.
// The class factory in turn passes the CreateInstance through to
// PolicyStatusMachineServiceClass. If the CreateInstance fails, it instantiates
// PolicyStatusMachineFallbackClass instead.
typedef BrokerClassFactoryRegistrar<__uuidof(PolicyStatusMachineServiceClass),
                                    __uuidof(PolicyStatusMachineFallbackClass),
                                    __uuidof(PolicyStatusMachineClass),
                                    kPolicyStatusMachineBrokerProgId>
                                    PolicyStatusMachineBroker;

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_BROKER_CLASS_FACTORY_H_
