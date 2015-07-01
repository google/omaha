// Copyright 2012 Google Inc.
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
// Wraps AppCommand in a model object. See app_command.h for further details
// about application commands.

#ifndef OMAHA_GOOPDATE_APP_COMMAND_MODEL_H__
#define OMAHA_GOOPDATE_APP_COMMAND_MODEL_H__

#include <atlbase.h>
#include <atlcom.h>
#include <windows.h>
#include <string>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/constants.h"
#include "omaha/goopdate/app_bundle.h"  // Required for com_wrapper_creator
#include "omaha/goopdate/com_wrapper_creator.h"
#include "omaha/goopdate/model_object.h"

namespace omaha {

class AppCommand;
class AppCommandVerifier;

// Loads, provides metadata for, and executes named commands for installed
// apps.
class AppCommandModel : public ModelObject {
 public:
  ~AppCommandModel();

  // Instantiates a model object corresponding to the identified application
  // command. Fails if the command is undefined.
  static HRESULT Load(App* app,
                      const CString& cmd_id,
                      AppCommandModel** app_command);

  // Instantiates a model object corresponding to the identified app and
  // previously loaded command.
  AppCommandModel(App* app, AppCommand* app_command);

  // Executes the command at the current integrity level. If successful,
  // the caller is responsible for closing the process HANDLE. This method does
  // not enforce the 'web accessible' constraint (this is the caller's
  // responsibility).
  // |verifier| is optional and, if provided, will be used to verify the safety
  // of the executable implementing the command.
  // |parameters| are positional parameters to the command and must correspond
  // in size to the number of expected parameters (command-defined).
  HRESULT Execute(AppCommandVerifier* verifier,
                  const std::vector<CString>& parameters,
                  HANDLE* process);

  // Returns the status of the last execution of this instance, or
  // COMMAND_STATUS_INIT if none. See omaha3_idl.idl for definition of
  // AppCommandStatus.
  AppCommandStatus GetStatus();

  // Returns the exit code if status is COMMAND_STATUS_COMPLETE. Otherwise,
  // returns MAXDWORD.
  DWORD GetExitCode();

  // Returns the command output if status is COMMAND_STATUS_COMPLETE. Otherwise,
  // returns an empty string.
  CString GetOutput();

  // Returns true if this command is allowed to be invoked through the
  // OneClick control.
  bool is_web_accessible() const;

 private:
  scoped_ptr<AppCommand> app_command_;

  DISALLOW_COPY_AND_ASSIGN(AppCommandModel);
};

class ATL_NO_VTABLE AppCommandWrapper
    : public ComWrapper<AppCommandWrapper, AppCommandModel>,
      public IDispatchImpl<IAppCommand2,
                           &__uuidof(IAppCommand2),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion> {
 public:
  // IAppCommand.
  STDMETHOD(get_isWebAccessible)(VARIANT_BOOL* is_web_accessible);
  STDMETHOD(get_status)(UINT* status);
  STDMETHOD(get_exitCode)(DWORD* exit_code);
  STDMETHOD(execute)(VARIANT arg1,
                     VARIANT arg2,
                     VARIANT arg3,
                     VARIANT arg4,
                     VARIANT arg5,
                     VARIANT arg6,
                     VARIANT arg7,
                     VARIANT arg8,
                     VARIANT arg9);

  // IAppCommand2
  STDMETHOD(get_output)(BSTR* output);

 protected:
  AppCommandWrapper() {}
  virtual ~AppCommandWrapper() {}

  BEGIN_COM_MAP(AppCommandWrapper)
    COM_INTERFACE_ENTRY(IAppCommand2)
    COM_INTERFACE_ENTRY(IAppCommand)
    COM_INTERFACE_ENTRY(IDispatch)
  END_COM_MAP()

 private:
  DISALLOW_COPY_AND_ASSIGN(AppCommandWrapper);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_COMMAND_MODEL_H__
