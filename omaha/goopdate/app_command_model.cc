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

#include "omaha/goopdate/app_command_model.h"

#include "OleAuto.h"

#include "base/file.h"
#include "base/utils.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/goopdate/app.h"
#include "omaha/goopdate/app_bundle.h"
#include "omaha/goopdate/app_command.h"
#include "omaha/goopdate/app_command_configuration.h"
#include "omaha/goopdate/google_app_command_verifier.h"
#include "omaha/goopdate/model.h"

namespace omaha {

AppCommandModel::AppCommandModel(App* app, AppCommand* app_command)
  : ModelObject(app->model()),
    app_command_(app_command) {
}

AppCommandModel::~AppCommandModel() {
  ASSERT1(model()->IsLockedByCaller());
}

HRESULT AppCommandModel::Load(App* app,
                              const CString& cmd_id,
                              AppCommandModel** app_command_model) {
  ASSERT1(app);
  ASSERT1(app_command_model);

  scoped_ptr<AppCommandConfiguration> configuration;

  HRESULT hr = AppCommandConfiguration::Load(GuidToString(app->app_guid()),
                                             app->app_bundle()->is_machine(),
                                             cmd_id,
                                             address(configuration));
  if (FAILED(hr)) {
    return hr;
  }

  *app_command_model = new AppCommandModel(
      app, configuration->Instantiate(app->app_bundle()->session_id()));

  return S_OK;
}

HRESULT AppCommandModel::Execute(AppCommandVerifier* verifier,
                                 const std::vector<CString>& parameters,
                                 HANDLE* process) {
  __mutexScope(model()->lock());
  return app_command_->Execute(verifier, parameters, process);
}

AppCommandStatus AppCommandModel::GetStatus() {
  __mutexScope(model()->lock());
  return app_command_->GetStatus();
}

DWORD AppCommandModel::GetExitCode() {
  __mutexScope(model()->lock());
  return app_command_->GetExitCode();
}

CString AppCommandModel::GetOutput() {
  __mutexScope(model()->lock());
  return app_command_->GetOutput();
}

bool AppCommandModel::is_web_accessible() const {
  __mutexScope(model()->lock());
  return app_command_->is_web_accessible();
}

STDMETHODIMP AppCommandWrapper::get_isWebAccessible(
    VARIANT_BOOL* is_web_accessible) {
  __mutexScope(model()->lock());
  *is_web_accessible = wrapped_obj()->is_web_accessible();
  return S_OK;
}

STDMETHODIMP AppCommandWrapper::get_status(UINT* status) {
  __mutexScope(model()->lock());
  *status = wrapped_obj()->GetStatus();
  return S_OK;
}

STDMETHODIMP AppCommandWrapper::get_exitCode(DWORD* exit_code) {
  __mutexScope(model()->lock());
  *exit_code = wrapped_obj()->GetExitCode();
  return S_OK;
}

STDMETHODIMP AppCommandWrapper::get_output(BSTR* output) {
  __mutexScope(model()->lock());
  *output = wrapped_obj()->GetOutput().AllocSysString();
  return S_OK;
}

namespace {

// Extracts a BSTR from a VARIANT.  Returns the inner BSTR if the VARIANT is
// VT_BSTR or VT_BSTR | VT_BYREF; returns NULL if the VARIANT does not contain
// a string.
BSTR BStrFromVariant(const VARIANT& source) {
  if (V_VT(&source) == VT_BSTR) {
    return V_BSTR(&source);
  }
  if (V_VT(&source) == (VT_BSTR | VT_BYREF)) {
    return *(V_BSTRREF(&source));
  }
  return NULL;
}

}  // namespace

STDMETHODIMP AppCommandWrapper::execute(VARIANT arg1,
                                        VARIANT arg2,
                                        VARIANT arg3,
                                        VARIANT arg4,
                                        VARIANT arg5,
                                        VARIANT arg6,
                                        VARIANT arg7,
                                        VARIANT arg8,
                                        VARIANT arg9) {
  __mutexScope(model()->lock());

  // Filled-in parameters should contain BSTR or BSTR-by-reference; unused
  // parameters should be VT_ERROR with scode DISP_E_PARAMNOTFOUND, which will
  // be converted to NULL.  (In theory, a COM object could pass us anything in
  // the Variants, but string types will be the only ones we will pass to the
  // formatter.)
  std::vector<CString> parameters_vector;
  parameters_vector.push_back(BStrFromVariant(arg1));
  parameters_vector.push_back(BStrFromVariant(arg2));
  parameters_vector.push_back(BStrFromVariant(arg3));
  parameters_vector.push_back(BStrFromVariant(arg4));
  parameters_vector.push_back(BStrFromVariant(arg5));
  parameters_vector.push_back(BStrFromVariant(arg6));
  parameters_vector.push_back(BStrFromVariant(arg7));
  parameters_vector.push_back(BStrFromVariant(arg8));
  parameters_vector.push_back(BStrFromVariant(arg9));

  GoogleAppCommandVerifier verifier;
  scoped_process process;
  return wrapped_obj()->Execute(
      &verifier, parameters_vector, address(process));
}

}  // namespace omaha
