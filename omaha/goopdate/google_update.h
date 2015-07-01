// Copyright 2009-2010 Google Inc.
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
// Contains GoogleUpdate class which is the ATL exe module for the local
// server that allows launching of the browser at medium integrity.

#ifndef OMAHA_GOOPDATE_GOOGLE_UPDATE_H_
#define OMAHA_GOOPDATE_GOOGLE_UPDATE_H_

#include <windows.h>
#include <atlbase.h>
#include "base/scoped_ptr.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"

namespace omaha {

// TODO(omaha): Perhaps a better approach might be to use Adapter classes to
// modify behavior instead of the mode being explicitly specified here. Consider
// when making future changes.
// It might also be a good idea to rename this class and file since it is more
// a boilerplate for COM servers than it is related to "Google Update" or the
// main Omaha 3 COM server since it als handles various brokers.
class GoogleUpdate : public CAtlExeModuleT<GoogleUpdate> {
 public:
  enum ComServerMode {
    kUpdate3Mode,
    kBrokerMode,
    kOnDemandMode,
  };

  DECLARE_LIBID(LIBID_GoogleUpdate3Lib)

  explicit GoogleUpdate(bool is_machine, ComServerMode mode);
  ~GoogleUpdate();
  HRESULT RegisterClassObjects(DWORD cls_ctx, DWORD flags) throw();
  HRESULT RevokeClassObjects() throw();

  // The base ATL classes have an implicit dependency on the second parameter
  // to RegisterServer and UnregisterServer having a default value.
  HRESULT RegisterServer(BOOL register_tlb, const CLSID* id = NULL) throw();
  HRESULT UnregisterServer(BOOL unregister_tlb, const CLSID* id = NULL) throw();
  HRESULT PreMessageLoop(int show_cmd) throw();
  HRESULT PostMessageLoop() throw();
  HRESULT Main();

  // This is cloned from CAtlExeModuleT.Lock(). The one difference is the call
  // to ::CoAddRefServerProcess(). See the description for Unlock() below for
  // further information.
  virtual LONG Lock() throw() {
    ::CoAddRefServerProcess();
    LONG lock_count = CComGlobalsThreadModel::Increment(&m_nLockCnt);
    CORE_LOG(L6, (_T("[GoogleUpdate::Lock][%d]"), lock_count));
    return lock_count;
  }

  // This is cloned from CAtlExeModuleT.Unlock(). The big difference is the call
  // to ::CoReleaseServerProcess(), to ensure that the class factories are
  // suspended once the lock count drops to zero. This fixes a race condition
  // where an activation request could come in in the middle of shutting down.
  // This shutdown mechanism works with free threaded servers.
  //
  // There are race issues with the ATL  delayed shutdown mechanism, hence the
  // associated code has been eliminated, and we have an assert to make sure
  // m_bDelayShutdown is not set.
  virtual LONG Unlock() throw() {
    ASSERT1(!m_bDelayShutdown);

    ::CoReleaseServerProcess();
    LONG lock_count = CComGlobalsThreadModel::Decrement(&m_nLockCnt);
    CORE_LOG(L6, (_T("[GoogleUpdate::Unlock][%d]"), lock_count));

    if (lock_count == 0) {
      ::PostThreadMessage(m_dwMainThreadID, WM_QUIT, 0, 0);
    }

    return lock_count;
  }

 private:
  _ATL_OBJMAP_ENTRY* GetObjectMap();
  HRESULT RegisterOrUnregisterExe(bool is_register);
  static HRESULT RegisterOrUnregisterExe(void* data, bool is_register);

  ComServerMode mode_;
  bool is_machine_;

  DISALLOW_EVIL_CONSTRUCTORS(GoogleUpdate);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOGLE_UPDATE_H_

