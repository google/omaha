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
// Contains WorkerComWrapperShutdownCallBack which implements the
// shutdown handling code for the OnDemandUpdate COM Server.

#ifndef OMAHA_GOOPDATE_COM_WRAPPER_SHUTDOWN_HANDLER_H__
#define OMAHA_GOOPDATE_COM_WRAPPER_SHUTDOWN_HANDLER_H__

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include "omaha/common/debug.h"
#include "omaha/common/shutdown_callback.h"
#include "omaha/common/synchronized.h"

namespace omaha {

class WorkerComWrapperShutdownCallBack
    : public ShutdownCallback,
      public CComObjectRootEx<CComMultiThreadModel> {
 public:
  WorkerComWrapperShutdownCallBack(bool shutdown_on_final_release)
      : shutdown_on_final_release_(shutdown_on_final_release) {}
  virtual ~WorkerComWrapperShutdownCallBack() {}
  virtual HRESULT Shutdown();

  // Clears the ignore shutdown flag.
  int ReleaseIgnoreShutdown();

  // Atomically sets the ignore shutdown flag and returns its previous value.
  int AddRefIgnoreShutdown() {
    return InternalAddRef();
  }

  // Returns the value of the ignore shutdown flag.
  bool ShouldIgnoreShutdown() {
    InternalAddRef();
    return !!InternalRelease();
  }

 private:
  bool shutdown_on_final_release_;
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_COM_WRAPPER_SHUTDOWN_HANDLER_H__

