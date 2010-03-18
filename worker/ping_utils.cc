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


#include "omaha/worker/ping_utils.h"
#include <atlstr.h>
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/application_manager.h"
#include "omaha/worker/app_request.h"
#include "omaha/worker/app_request_data.h"
#include "omaha/worker/job.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/ping_event.h"
#include "omaha/worker/product_data.h"

namespace omaha {

namespace ping_utils {

// If version is NULL, the current version will be used.
HRESULT SendGoopdatePing(bool is_machine,
                         const CommandLineExtraArgs& extra_args,
                         const CString& install_source,
                         PingEvent::Types type,
                         HRESULT result,
                         int extra_code1,
                         const TCHAR* version,
                         Ping* ping) {
  CORE_LOG(L2, (_T("[SendGoopdatePing]")));
  ASSERT1(ping);

  CString previous_version;
  AppRequestData app_request_data_goopdate;
  BuildGoogleUpdateAppRequestData(is_machine,
                                  extra_args,
                                  install_source,
                                  &previous_version,
                                  &app_request_data_goopdate);

  PingEvent::Results event_result = (S_OK == result) ?
                                    PingEvent::EVENT_RESULT_SUCCESS :
                                    PingEvent::EVENT_RESULT_ERROR;

  PingEvent ping_event(type,
                       event_result,
                       result,
                       extra_code1,
                       previous_version);

  app_request_data_goopdate.AddPingEvent(ping_event);

  Request request(is_machine);
  if (version) {
    request.set_version(version);
  }

  AppRequest app_request(app_request_data_goopdate);
  request.AddAppRequest(app_request);

  HRESULT hr = ping->SendPing(&request);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[SendPing failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

// Initializes an AppRequest structure with information about
// GoogleUpdate. If extra contains data, its values are used. Otherwise,
// attempts to obtain data from the registry.
void BuildGoogleUpdateAppRequestData(bool is_machine,
                                     const CommandLineExtraArgs& extra_args,
                                     const CString& install_source,
                                     CString* previous_version,
                                     AppRequestData* app_request_data) {
  CORE_LOG(L3, (_T("[Ping::BuildGoogleUpdateAppRequest]")));

  ASSERT1(app_request_data);

  if (previous_version) {
    *previous_version = _T("");
  }

  AppData app_data_goopdate(kGoopdateGuid, is_machine);
  app_data_goopdate.set_version(GetVersionString());

  AppManager app_manager(is_machine);
  ProductData product_data;
  // TODO(omaha): Should we just call ReadAppDataFromStore?
  HRESULT hr = app_manager.ReadProductDataFromStore(kGoopdateGuid,
                                                    &product_data);
  if (SUCCEEDED(hr)) {
    if (previous_version) {
      *previous_version = product_data.app_data().previous_version();
    }
    app_data_goopdate.set_iid(product_data.app_data().iid());
    app_data_goopdate.set_brand_code(product_data.app_data().brand_code());
    app_data_goopdate.set_client_id(product_data.app_data().client_id());
    app_data_goopdate.set_did_run(product_data.app_data().did_run());
  } else {
    CORE_LOG(LEVEL_WARNING, (_T("[ReadProductDataFromStore failed]")
                             _T("[0x%x][%s]"), hr, kGoogleUpdateAppId));

    // Use branding data from the command line if present.
    // We do not want to use branding data from the command line if this is not
    // the first install of Google Update.
    if (!extra_args.brand_code.IsEmpty()) {
      app_data_goopdate.set_brand_code(extra_args.brand_code);
    }
    if (!extra_args.client_id.IsEmpty()) {
      app_data_goopdate.set_client_id(extra_args.client_id);
    }
  }

  // Always use the installation ID and install source from the command line if
  // present.
  if (GUID_NULL != extra_args.installation_id) {
    app_data_goopdate.set_iid(extra_args.installation_id);
  }
  app_data_goopdate.set_install_source(install_source);

  AppRequestData app_request_data_temp(app_data_goopdate);
  *app_request_data = app_request_data_temp;
}

HRESULT SendPostSetupPing(HRESULT result,
                          int extra_code1,
                          const CString& previous_version,
                          bool is_machine,
                          bool is_self_update,
                          const CommandLineExtraArgs& extra,
                          const CString& install_source,
                          Ping* ping) {
  CORE_LOG(L3, (_T("[Ping::SendPostSetupPing]")));
  ASSERT1(ping);

  scoped_ptr<Request> request(new Request(is_machine));
  AppRequestData app_request_data;
  BuildGoogleUpdateAppRequestData(
      is_machine,
      extra,
      install_source,
      NULL,
      &app_request_data);

  const PingEvent::Results event_result = (S_OK == result) ?
                                          PingEvent::EVENT_RESULT_SUCCESS :
                                          PingEvent::EVENT_RESULT_ERROR;
  const PingEvent::Types type = is_self_update ?
                                PingEvent::EVENT_SETUP_UPDATE_COMPLETE :
                                PingEvent::EVENT_SETUP_INSTALL_COMPLETE;

  PingEvent ping_event(type,
                       event_result,
                       result,
                       extra_code1,
                       previous_version);
  app_request_data.AddPingEvent(ping_event);

  if (is_self_update) {
    // In case of self updates we also indicate that we completed the update
    // job started by the previous version of Omaha.
    PingEvent ping_event2(PingEvent::EVENT_UPDATE_COMPLETE,
                          event_result,
                          result,
                          extra_code1,
                          previous_version);
    app_request_data.AddPingEvent(ping_event2);
  }

  AppRequest app_request(app_request_data);
  request->AddAppRequest(app_request);

  HRESULT hr = ping->SendPing(request.get());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[SendSetupPing(completed) failed][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}


PingEvent::Results CompletionStatusToPingEventResult(
    JobCompletionStatus status) {
  PingEvent::Results result = PingEvent::EVENT_RESULT_ERROR;
  switch (status) {
    case COMPLETION_SUCCESS:
      result = PingEvent::EVENT_RESULT_SUCCESS;
      break;
    case COMPLETION_SUCCESS_REBOOT_REQUIRED:
      result = PingEvent::EVENT_RESULT_SUCCESS_REBOOT;
      break;
    case COMPLETION_ERROR:
      result = PingEvent::EVENT_RESULT_ERROR;
      break;
    case COMPLETION_INSTALLER_ERROR_MSI:
      result = PingEvent::EVENT_RESULT_INSTALLER_ERROR_MSI;
      break;
    case COMPLETION_INSTALLER_ERROR_SYSTEM:
      result = PingEvent::EVENT_RESULT_INSTALLER_ERROR_SYSTEM;
      break;
    case COMPLETION_INSTALLER_ERROR_OTHER:
      result = PingEvent::EVENT_RESULT_INSTALLER_ERROR_OTHER;
      break;
    case COMPLETION_CANCELLED:
      result = PingEvent::EVENT_RESULT_CANCELLED;
      break;
    default:
      ASSERT1(false);
      break;
  }
  return result;
}

HRESULT BuildCompletedPingForAllProducts(const ProductDataVector& products,
                                         bool is_update,
                                         const CompletionInfo& info,
                                         Request* request) {
  CORE_LOG(L2, (_T("[BuildCompletedPingForAllProducts]")));
  ASSERT1(request);

  PingEvent::Types type = is_update ? PingEvent::EVENT_UPDATE_COMPLETE :
                                      PingEvent::EVENT_INSTALL_COMPLETE;
  PingEvent::Results result = CompletionStatusToPingEventResult(info.status);
  for (size_t i = 0; i < products.size(); ++i) {
    const ProductData& product_data = products[i];
    AppRequestData app_request_data(product_data.app_data());

    // Create and add the ping event.
    CString previous_version = app_request_data.app_data().previous_version();
    // TODO(omaha): Remove this value when circular log buffer is implemented.
    // This value must not be a valid Job::JobState.
    const int kAppProductsPingJobState = 0xff;
    PingEvent ping_event(type,
                         result,
                         info.error_code,
                         Job::kJobStateExtraCodeMask | kAppProductsPingJobState,
                         previous_version);
    app_request_data.AddPingEvent(ping_event);

    AppRequest app_request(app_request_data);
    request->AddAppRequest(app_request);
  }
  ASSERT1(products.size() ==
          static_cast<size_t>(request->get_request_count()));
  return S_OK;
}

HRESULT SendCompletedPingsForAllProducts(const ProductDataVector& products,
                                         bool is_machine,
                                         bool is_update,
                                         const CompletionInfo& info,
                                         Ping* ping) {
  CORE_LOG(L2, (_T("[SendCompletedPingsForAllProducts]")));
  ASSERT1(ping);

  scoped_ptr<Request> request(new Request(is_machine));
  HRESULT hr = BuildCompletedPingForAllProducts(products,
                                                is_update,
                                                info,
                                                request.get());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[BuildCompletedPingForAllProducts failed][0x%08x]"), hr));
    return hr;
  }

  if (!products.empty()) {
    HRESULT hr = ping->SendPing(request.get());
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[SendPing failed][0x%08x]"), hr));
      return hr;
    }
  }

  return S_OK;
}

}  // namespace ping_utils

}  // namespace omaha

