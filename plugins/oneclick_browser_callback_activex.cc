// Copyright 2007-2009 Google Inc.
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
// OneClick callback helper for ActiveX browsers.

#include "omaha/plugins/oneclick_browser_callback_activex.h"

#include <dispex.h>
#include <winhttp.h>
#include <atlbase.h>
#include <atlcom.h>

#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/goopdate/goopdate_utils.h"

// Note:  IE gives a dispinterface JScriptTypeInfo as our onxxxxx property
// values. classid: C59C6B12-F6C1-11CF-8835-00A0C911E8B2
// We invoke "dispatch id 0" to call the associated function.
// See http://www.ddj.com/windows/184404200

// We prefer to call things thru IDispatchEx in order to use DISPID_THIS.
// Note: strangely, the parameters passed thru IDispatch are in reverse order as
// compared to the method signature being invoked.

namespace omaha {

OneClickBrowserCallbackActiveX::OneClickBrowserCallbackActiveX() {
}

OneClickBrowserCallbackActiveX::~OneClickBrowserCallbackActiveX() {
}

HRESULT OneClickBrowserCallbackActiveX::Initialize(VARIANT* success_callback,
                                                   VARIANT* failure_callback) {
  // These two parameters are optional, but if they're valid VARIANTs then they
  // need to be of type VT_DISPATCH or we fail.

  if (!VariantIsNullOrUndefined(success_callback)) {
    if (success_callback->vt == VT_DISPATCH) {
      success_callback_dispatch_ = success_callback->pdispVal;
    } else {
      return E_INVALIDARG;
    }
  }

  if (!VariantIsNullOrUndefined(failure_callback)) {
    if (failure_callback->vt == VT_DISPATCH) {
      failure_callback_dispatch_ = failure_callback->pdispVal;
    } else {
      return E_INVALIDARG;
    }
  }

  return S_OK;
}

bool OneClickBrowserCallbackActiveX::VariantIsNullOrUndefined(
    const VARIANT* var) {
  return var->vt == VT_NULL || var->vt == VT_EMPTY;
}

void OneClickBrowserCallbackActiveX::DoSuccessCallback() {
  CORE_LOG(L2, (_T("[DoSuccessCallback entered]")));

  if (!success_callback_dispatch_) {
    return;
  }

  const DISPID kDispId0 = 0;

  HRESULT hr = 0;
  DISPPARAMS dispparams = {0};
  CComQIPtr<IDispatchEx> dispatchex =
      static_cast<IDispatch*>(success_callback_dispatch_);

  if (dispatchex) {
    DISPID disp_this = DISPID_THIS;
    VARIANT var[1];
    var[0].vt = VT_DISPATCH;
    var[0].pdispVal = dispatchex;

    dispparams.rgvarg = var;
    dispparams.rgdispidNamedArgs = &disp_this;
    dispparams.cNamedArgs = 1;
    dispparams.cArgs = 1;

    hr = dispatchex->InvokeEx(kDispId0, LOCALE_USER_DEFAULT,
                              DISPATCH_METHOD, &dispparams,
                              NULL, NULL, NULL);
  } else if (success_callback_dispatch_) {
    // Fallback on IDispatch if needed.
    UINT arg_err = 0;
    dispparams.cArgs = 0;

    hr = success_callback_dispatch_->Invoke(kDispId0,
                                            IID_NULL,
                                            LOCALE_SYSTEM_DEFAULT,
                                            DISPATCH_METHOD,
                                            &dispparams,
                                            NULL,
                                            NULL,
                                            &arg_err);
  }
}

void OneClickBrowserCallbackActiveX::DoFailureCallback(HRESULT hr_error) {
  CORE_LOG(L2, (_T("[DoFailureCallback entered][hr_error=0x%x]"), hr_error));

  if (!failure_callback_dispatch_) {
    return;
  }

  const DISPID kDispId0 = 0;

  HRESULT hr = 0;
  DISPPARAMS dispparams = {0};
  CComQIPtr<IDispatchEx> dispatchex =
      static_cast<IDispatch*>(failure_callback_dispatch_);

  if (dispatchex) {
    DISPID disp_this = DISPID_THIS;
    VARIANT var[2];
    var[0].vt = VT_DISPATCH;
    var[0].pdispVal = dispatchex;
    var[1].vt = VT_I4;
    var[1].intVal = hr_error;

    dispparams.rgvarg = var;
    dispparams.rgdispidNamedArgs = &disp_this;
    dispparams.cNamedArgs = 1;
    dispparams.cArgs = 2;

    hr = dispatchex->InvokeEx(kDispId0, LOCALE_USER_DEFAULT,
                              DISPATCH_METHOD, &dispparams,
                              NULL, NULL, NULL);
  } else if (failure_callback_dispatch_) {
    // Fallback on IDispatch if needed.
    UINT arg_err = 0;
    VARIANT var[1];
    var[0].vt = VT_I4;
    var[0].intVal = hr_error;

    dispparams.rgvarg = var;
    dispparams.cArgs = 1;

    hr = failure_callback_dispatch_->Invoke(kDispId0,
                                            IID_NULL,
                                            LOCALE_SYSTEM_DEFAULT,
                                            DISPATCH_METHOD,
                                            &dispparams,
                                            NULL,
                                            NULL,
                                            &arg_err);
  }
}

}  // namespace omaha

