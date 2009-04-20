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

#include "omaha/worker/job_creator.h"

#include <windows.h>
#include <atlcom.h>
#include <cstring>
#include <map>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/time.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_xml_parser.h"
#include "omaha/goopdate/request.h"
#include "omaha/goopdate/resource.h"
#include "omaha/goopdate/update_response_data.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/worker_event_logger.h"
#include "omaha/worker/worker_metrics.h"

namespace omaha {

HRESULT JobCreator::CreateJobsFromResponses(
    const UpdateResponses& responses,
    const ProductDataVector& products,
    Jobs* jobs,
    Request* ping_request,
    CString* event_log_text,
    CompletionInfo* completion_info) {
  ASSERT1(jobs);
  ASSERT1(ping_request);
  ASSERT1(event_log_text);
  ASSERT1(completion_info);
  CORE_LOG(L2, (_T("[JobCreator::CreateJobsFromResponses]")));

  // First check to see if GoogleUpdate update is available, if so we only
  // create the job for googleupdate and don't create jobs for the rest.
  if (IsGoopdateUpdateAvailable(responses)) {
    return CreateGoopdateJob(responses,
                             products,
                             jobs,
                             ping_request,
                             event_log_text,
                             completion_info);
  }

  return CreateJobsFromResponsesInternal(responses,
                                         products,
                                         jobs,
                                         ping_request,
                                         event_log_text,
                                         completion_info);
}

bool JobCreator::IsGoopdateUpdateAvailable(const UpdateResponses& responses) {
  UpdateResponses::const_iterator iter = responses.find(kGoopdateGuid);
  if (iter == responses.end()) {
    return false;
  }

  const UpdateResponse& update_response = iter->second;
  return (_tcsicmp(kResponseStatusOkValue,
                   update_response.update_response_data().status()) == 0);
}

void JobCreator::AddAppUpdateDeferredPing(const AppData& product_data,
                                          Request* ping_request) {
  ASSERT1(ping_request);

  AppRequest ping_app_request;
  AppRequestData ping_app_request_data(product_data);
  PingEvent ping_event(PingEvent::EVENT_UPDATE_COMPLETE,
                       PingEvent::EVENT_RESULT_UPDATE_DEFERRED,
                       0,
                       0,  // extra code 1
                       product_data.previous_version());
  ping_app_request_data.AddPingEvent(ping_event);
  ping_app_request.set_request_data(ping_app_request_data);
  ping_request->AddAppRequest(ping_app_request);
}

HRESULT JobCreator::CreateGoopdateJob(const UpdateResponses& responses,
                                      const ProductDataVector& products,
                                      Jobs* jobs,
                                      Request* ping_request,
                                      CString* event_log_text,
                                      CompletionInfo* completion_info) {
  ASSERT1(jobs);
  ASSERT1(ping_request);
  ASSERT1(event_log_text);
  ASSERT1(completion_info);

  UpdateResponses::const_iterator it = responses.find(kGoopdateGuid);
  if (it == responses.end()) {
    return E_INVALIDARG;
  }

  UpdateResponses goopdate_responses;
  goopdate_responses[kGoopdateGuid] = it->second;

  // Create a job for GoogleUpdate only and defer updates for other products,
  // if updates are available.
  ProductDataVector goopdate_products;
  bool is_other_app_update_available(false);
  for (size_t i = 0; i < products.size(); ++i) {
    const GUID app_id = products[i].app_data().app_guid();
    if (::IsEqualGUID(app_id, kGoopdateGuid)) {
      goopdate_products.push_back(products[i]);
    } else {
      it = responses.find(app_id);
      if (it != responses.end() && IsUpdateAvailable(it->second)) {
        is_other_app_update_available = true;
        AddAppUpdateDeferredPing(products[i].app_data(), ping_request);
      }
    }
  }
  ASSERT1(goopdate_products.size() == 1);

  if (is_other_app_update_available) {
    ++metric_worker_skipped_app_update_for_self_update;
  }

  return CreateJobsFromResponsesInternal(goopdate_responses,
                                         goopdate_products,
                                         jobs,
                                         ping_request,
                                         event_log_text,
                                         completion_info);
}

HRESULT JobCreator::ReadOfflineManifest(const CString& offline_dir,
                                        const CString& app_guid,
                                        UpdateResponse* response) {
  ASSERT1(response);

  CString manifest_filename = app_guid + _T(".gup");
  CString manifest_path = ConcatenatePath(offline_dir, manifest_filename);
  if (!File::Exists(manifest_path)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  UpdateResponses responses;
  HRESULT hr = GoopdateXmlParser::ParseManifestFile(manifest_path, &responses);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[Could not parse manifest][%s]"), manifest_path));
    return hr;
  }
  ASSERT1(!responses.empty());
  ASSERT1(1 == responses.size());

  UpdateResponses::const_iterator iter = responses.begin();
  *response = iter->second;
  ASSERT1(!app_guid.CompareNoCase(GuidToString(
      response->update_response_data().guid())));
  return S_OK;
}

HRESULT JobCreator::FindOfflineFilePath(const CString& offline_dir,
                                        const CString& app_guid,
                                        CString* file_path) {
  ASSERT1(file_path);
  file_path->Empty();

  CString offline_app_dir = ConcatenatePath(offline_dir, app_guid);
  CString pattern(_T("*"));
  std::vector<CString> files;
  HRESULT hr = FindFiles(offline_app_dir, pattern, &files);
  if (FAILED(hr)) {
    return hr;
  }

  CString local_file_path;
  // Skip over "." and "..".
  size_t i = 0;
  for (; i < files.size(); ++i) {
    local_file_path = ConcatenatePath(offline_app_dir, files[i]);
    if (!File::IsDirectory(local_file_path)) {
      break;
    }
  }
  if (i == files.size()) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  *file_path = local_file_path;
  return S_OK;
}

HRESULT JobCreator::CreateOfflineJobs(const CString& offline_dir,
                                      const ProductDataVector& products,
                                      Jobs* jobs,
                                      Request* ping_request,
                                      CString* event_log_text,
                                      CompletionInfo* completion_info) {
  CORE_LOG(L2, (_T("[JobCreator::CreateOfflineJobs]")));
  ASSERT1(jobs);
  ASSERT1(ping_request);
  ASSERT1(event_log_text);
  ASSERT1(completion_info);

  ProductDataVector::const_iterator products_it = products.begin();
  for (; products_it != products.end(); ++products_it) {
    const ProductData& product_data = *products_it;
    CString guid(GuidToString(product_data.app_data().app_guid()));
    UpdateResponse response;
    CString file_path;

    HRESULT hr = ReadOfflineManifest(offline_dir, guid, &response);
    if (SUCCEEDED(hr)) {
      hr = FindOfflineFilePath(offline_dir, guid, &file_path);
    }
    if (SUCCEEDED(hr)) {
      hr = goopdate_utils::ValidateDownloadedFile(
               file_path,
               response.update_response_data().hash(),
               static_cast<uint32>(response.update_response_data().size()));
    }
    if (FAILED(hr)) {
      *completion_info = CompletionInfo(COMPLETION_ERROR, hr, _T(""));
      return hr;
    }

    Job* product_job = NULL;
    hr = HandleProductUpdateIsAvailable(product_data,
                                        response,
                                        &product_job,
                                        ping_request,
                                        event_log_text,
                                        completion_info);
    if (FAILED(hr)) {
      return hr;
    }

    product_job->set_download_file_name(file_path);
    jobs->push_back(product_job);
  }

  return S_OK;
}

HRESULT JobCreator::CreateJobsFromResponsesInternal(
    const UpdateResponses& responses,
    const ProductDataVector& products,
    Jobs* jobs,
    Request* ping_request,
    CString* event_log_text,
    CompletionInfo* completion_info) {
  ASSERT1(jobs);
  ASSERT1(ping_request);
  ASSERT1(event_log_text);
  ASSERT1(completion_info);
  CORE_LOG(L2, (_T("[JobCreator::CreateJobsFromResponsesInternal]")));

  // The ordering needs to be done based on the products array, instead of
  // basing this on the responses. This is because the first element in the
  // job that is created is considered the primary app.
  ProductDataVector::const_iterator products_it = products.begin();
  for (; products_it != products.end(); ++products_it) {
    const ProductData& product_data = *products_it;
    AppData product_app_data = product_data.app_data();

    UpdateResponses::const_iterator iter =
        responses.find(product_app_data.app_guid());
    if (iter == responses.end()) {
      AppRequestData ping_app_request_data(product_app_data);
      HandleResponseNotFound(product_app_data,
                             &ping_app_request_data,
                             event_log_text);
      continue;
    }

    const UpdateResponse& response = iter->second;

    AppManager app_manager(is_machine_);
    // If this is an update job, need to call
    // AppManager::UpdateApplicationState() to make sure the registry is
    // initialized properly.  Need to do this within HandleComponents also
    // for each of the components.
    if (is_update_) {
      app_manager.UpdateApplicationState(&product_app_data);
    }

    // Write the TT Token, if any, with what the server returned. The server
    // may ask us to clear the token as well.
    app_manager.WriteTTToken(product_app_data, response.update_response_data());


    if (IsUpdateAvailable(response)) {
      ProductData modified_product_data(product_data);
      modified_product_data.set_app_data(product_app_data);
      Job* product_job = NULL;
      HRESULT hr = HandleProductUpdateIsAvailable(modified_product_data,
                                                  response,
                                                  &product_job,
                                                  ping_request,
                                                  event_log_text,
                                                  completion_info);
      if (FAILED(hr)) {
        return hr;
      }

      jobs->push_back(product_job);
    } else {
      AppRequest ping_app_request;
      AppRequestData ping_app_request_data(product_app_data);
      HRESULT hr = HandleUpdateNotAvailable(product_app_data,
                                            response.update_response_data(),
                                            &ping_app_request_data,
                                            event_log_text,
                                            completion_info);
      // TODO(omaha): It's unclear why the ping is added before the FAILED
      // check here but after it in HandleProductUpdateIsAvailable(). Is this
      // intentional?
      ping_app_request.set_request_data(ping_app_request_data);
      ping_request->AddAppRequest(ping_app_request);
      if (FAILED(hr)) {
        return hr;
      }
    }
  }

  return S_OK;
}

HRESULT JobCreator::HandleProductUpdateIsAvailable(
    const ProductData& product_data,
    const UpdateResponse& response,
    Job** product_job,
    Request* ping_request,
    CString* event_log_text,
    CompletionInfo* completion_info) {
  ASSERT1(product_job);

  AppData product_app_data = product_data.app_data();
  AppRequest ping_app_request;
  AppRequestData ping_app_request_data(product_app_data);

  if (is_update_) {
    // previous_version should have been populated above.
    ASSERT1(!product_app_data.previous_version().IsEmpty());
  } else {
    // Copy the current version of app, if available, to previous version
    // in the AppData object so it is sent in pings.
    AppData existing_app_data;
    AppManager app_manager(is_machine_);
    HRESULT hr = app_manager.ReadAppDataFromStore(
                     GUID_NULL,
                     product_app_data.app_guid(),
                     &existing_app_data);
    if (SUCCEEDED(hr)) {
      product_app_data.set_previous_version(existing_app_data.version());
    }
  }

  ASSERT1(::IsEqualGUID(GUID_NULL, product_app_data.parent_app_guid()));
  HandleUpdateIsAvailable(product_app_data,
                          response.update_response_data(),
                          &ping_app_request_data,
                          event_log_text,
                          product_job);
  ASSERT1(*product_job);
  ping_app_request.set_request_data(ping_app_request_data);

  HRESULT hr = HandleComponents(product_data,
                                response,
                                &ping_app_request,
                                event_log_text,
                                *product_job,
                                completion_info);
  if (FAILED(hr)) {
    return hr;
  }

  ping_request->AddAppRequest(ping_app_request);
  return S_OK;
}

void JobCreator::HandleResponseNotFound(const AppData& app_data,
                                        AppRequestData* ping_app_request_data,
                                        CString* event_log_text) {
  ASSERT1(event_log_text);
  ASSERT1(ping_app_request_data);

  PingEvent::Types event_type = is_update_ ?
                                PingEvent::EVENT_UPDATE_COMPLETE :
                                PingEvent::EVENT_INSTALL_COMPLETE;
  PingEvent ping_event(event_type,
                       PingEvent::EVENT_RESULT_ERROR,
                       GOOPDATE_E_NO_SERVER_RESPONSE,
                       0,  // extra code 1
                       app_data.previous_version());
  ping_app_request_data->AddPingEvent(ping_event);

  event_log_text->AppendFormat(
      _T("App=%s, Ver=%s, Status=no-response-received\n"),
      GuidToString(app_data.app_guid()),
      app_data.version());
}

void JobCreator::HandleUpdateIsAvailable(
    const AppData& app_data,
    const UpdateResponseData& response_data,
    AppRequestData* ping_app_request_data,
    CString* event_log_text,
    Job** job) {
  ASSERT1(ping_app_request_data);
  ASSERT1(event_log_text);
  ASSERT1(job);

  ASSERT1(!app_data.is_update_disabled());
  if (app_data.is_update_disabled()) {
    CORE_LOG(LW, (_T("[Update returned for update-disabled app][%s]"),
                  GuidToString(app_data.app_guid())));
  }

  if (is_auto_update_) {
    if (::IsEqualGUID(kGoopdateGuid, app_data.app_guid())) {
      ++metric_worker_self_updates_available;
    } else {
      ++metric_worker_app_updates_available;
    }

    // Only record an update available event for updates.
    // We have other mechanisms, including IID, to track install success.
    AppManager app_manager(is_machine_);
    app_manager.UpdateUpdateAvailableStats(app_data.parent_app_guid(),
                                           app_data.app_guid());
  }

  // Ping and record events only for "real" jobs. On demand update checks only
  // jobs should not ping, since no update is actually applied.
  if (!is_update_check_only_) {
    PingEvent::Types event_type = is_update_ ?
                                  PingEvent::EVENT_UPDATE_APPLICATION_BEGIN :
                                  PingEvent::EVENT_INSTALL_APPLICATION_BEGIN;
    PingEvent ping_event(event_type,
                         PingEvent::EVENT_RESULT_SUCCESS,
                         0,  // error code
                         0,  // extra code 1
                         app_data.previous_version());
    ping_app_request_data->AddPingEvent(ping_event);
  }
  const TCHAR* status = is_update_check_only_ ? _T("check only") :
                        is_update_ ? _T("update") : _T("install");
  event_log_text->AppendFormat(_T("App=%s, Ver=%s, Status=%s\n"),
                               GuidToString(app_data.app_guid()),
                               app_data.version(),
                               status);

  // TODO(omaha): When components are implemented, how do we handle the case
  // that this is just a place holder and does not really need an update?
  // We may be in an app that only needs an update here because it's child
  // components need updates so we'll need to flag the job of that somehow.
  // Unless it's already tagged in the response_data (which does know that
  // there's nothing to download) but the job state will need to be updated.
  scoped_ptr<Job> new_job(new Job(is_update_, ping_));
  new_job->set_app_data(app_data);
  new_job->set_update_response_data(response_data);
  new_job->set_is_background(is_auto_update_);
  new_job->set_is_update_check_only(is_update_check_only_);

  *job = new_job.release();
}

HRESULT JobCreator::HandleUpdateNotAvailable(
    const AppData& app_data,
    const UpdateResponseData& response_data,
    AppRequestData* ping_app_request_data,
    CString* event_log_text,
    CompletionInfo* completion_info) {
  ASSERT1(ping_app_request_data);
  ASSERT1(event_log_text);
  ASSERT1(completion_info);

  PingEvent::Results result_type = PingEvent::EVENT_RESULT_ERROR;
  CompletionInfo info = UpdateResponseDataToCompletionInfo(
      response_data,
      ping_app_request_data->app_data().display_name());
  switch (info.error_code) {
    case static_cast<DWORD>(GOOPDATE_E_NO_UPDATE_RESPONSE):
      event_log_text->AppendFormat(
          _T("App=%s, Ver=%s, Status=no-update\n"),
          GuidToString(app_data.app_guid()),
          app_data.version());
      if (is_update_) {
        // In case of updates, a "noupdate" response is not an error.
        result_type = PingEvent::EVENT_RESULT_NOUPDATE;
        info.status = COMPLETION_SUCCESS;
        info.error_code = NOERROR;

        AppManager app_manager(is_machine_);
        app_manager.RecordSuccessfulUpdateCheck(app_data.parent_app_guid(),
                                                app_data.app_guid());
      }
      break;
    case static_cast<DWORD>(GOOPDATE_E_RESTRICTED_SERVER_RESPONSE):
      event_log_text->AppendFormat(
          _T("App=%s, Ver=%s, Status=restricted\n"),
          GuidToString(app_data.app_guid()),
          app_data.version());
      break;
    case GOOPDATE_E_UNKNOWN_APP_SERVER_RESPONSE:
    case GOOPDATE_E_INTERNAL_ERROR_SERVER_RESPONSE:
    case GOOPDATE_E_UNKNOWN_SERVER_RESPONSE:
    default:
      event_log_text->AppendFormat(_T("App=%s, Ver=%s, Status=error:0x%08x\n"),
                                   GuidToString(app_data.app_guid()),
                                   app_data.version(),
                                   info.error_code);
      break;
  }

  // Only ping for server responses other than "noupdate" or errors.
  if (result_type != PingEvent::EVENT_RESULT_NOUPDATE ||
      info.error_code != NOERROR) {
    PingEvent::Types event_type = is_update_ ?
                                  PingEvent::EVENT_UPDATE_COMPLETE :
                                  PingEvent::EVENT_INSTALL_COMPLETE;
    PingEvent ping_event(event_type,
                         result_type,
                         info.error_code,
                         0,  // extra code 1
                         app_data.previous_version());
    ping_app_request_data->AddPingEvent(ping_event);
  }

  if (fail_if_update_not_available_) {
    *completion_info = info;
    return info.error_code;
  }

  return S_OK;
}

HRESULT JobCreator::HandleComponents(
    const ProductData& product_data,
    const UpdateResponse& product_response,
    AppRequest* product_ping_request,
    CString* event_log_text,
    Job* product_job,
    CompletionInfo* completion_info) {
  ASSERT1(product_ping_request);
  ASSERT1(event_log_text);
  ASSERT1(product_job);
  ASSERT1(completion_info);
  ASSERT1(product_data.num_components() == product_response.num_components());

  UNREFERENCED_PARAMETER(product_job);

  if (product_data.num_components() == 0) {
    return S_OK;
  }

  AppDataVector::const_iterator component_it = product_data.components_begin();
  for (; component_it != product_data.components_end(); ++component_it) {
    AppData app_data_component = *component_it;
    ASSERT1(!::IsEqualGUID(GUID_NULL, app_data_component.parent_app_guid()));
    ASSERT1(::IsEqualGUID(product_data.app_data().app_guid(),
                          app_data_component.parent_app_guid()));

    // TODO(omaha): For now we are creating only the component request
    // however this has to be populated with ping information.
    AppRequestData ping_component_request_data(app_data_component);

    if (!product_response.IsComponentPresent(app_data_component.app_guid())) {
      HandleResponseNotFound(app_data_component,
                             &ping_component_request_data,
                             event_log_text);
    }

    const UpdateResponseData& component_response =
        product_response.GetComponentData(app_data_component.app_guid());

    // If this is an update job, need to call
    // AppManager::UpdateApplicationState() to make sure the registry is
    // initialized properly.
    if (is_update_) {
      AppManager app_manager(is_machine_);
      app_manager.UpdateApplicationState(&app_data_component);
    }

    if (IsUpdateAvailable(component_response)) {
      Job* component_job = NULL;
      HandleUpdateIsAvailable(app_data_component,
                              component_response,
                              &ping_component_request_data,
                              event_log_text,
                              &component_job);

      product_ping_request->AddComponentRequest(ping_component_request_data);
      // TODO(omaha): product_job->add_child_job(component_job);
    } else {
      HRESULT hr = HandleUpdateNotAvailable(app_data_component,
                                            component_response,
                                            &ping_component_request_data,
                                            event_log_text,
                                            completion_info);
      // TODO(omaha):  Do we need ping_request updated here with data?
      if (FAILED(hr)) {
        return hr;
      }
    }
  }
  return S_OK;
}

// app_name in UpdateResponseData is not filled in. See the TODO in that class.
CompletionInfo JobCreator::UpdateResponseDataToCompletionInfo(
    const UpdateResponseData& response_data,
    const CString& display_name) {
  CompletionInfo info;
  const CString& status = response_data.status();
  if (_tcsicmp(kResponseStatusOkValue, status) == 0) {
    info.status = COMPLETION_SUCCESS;
    info.error_code = 0;
  } else if (_tcsicmp(kResponseStatusNoUpdate, status) == 0) {
    // "noupdate" is considered an error but the calling code can map it to
    // a successful completion, in the cases of silent and on demand updates.
    info.status = COMPLETION_ERROR;
    info.error_code = static_cast<DWORD>(GOOPDATE_E_NO_UPDATE_RESPONSE);
    VERIFY1(info.text.LoadString(IDS_NO_UPDATE_RESPONSE));
  } else if (_tcsicmp(kResponseStatusRestrictedExportCountry, status) == 0) {
    // "restricted"
    info.status = COMPLETION_ERROR;
    info.error_code =
        static_cast<DWORD>(GOOPDATE_E_RESTRICTED_SERVER_RESPONSE);
    VERIFY1(info.text.LoadString(IDS_RESTRICTED_RESPONSE_FROM_SERVER));
  } else if (_tcsicmp(kResponseStatusUnKnownApplication, status) == 0) {
    // "error-UnKnownApplication"
    info.status = COMPLETION_ERROR;
    info.error_code =
        static_cast<DWORD>(GOOPDATE_E_UNKNOWN_APP_SERVER_RESPONSE);
    VERIFY1(info.text.LoadString(IDS_UNKNOWN_APPLICATION));
  } else if (_tcsicmp(kResponseStatusOsNotSupported, status) == 0) {
    // "error-OsNotSupported"
    info.status = COMPLETION_ERROR;
    info.error_code = static_cast<DWORD>(GOOPDATE_E_OS_NOT_SUPPORTED);
    if (response_data.error_url().IsEmpty()) {
      info.text.FormatMessage(IDS_NON_OK_RESPONSE_FROM_SERVER, status);
    } else {
      info.text.FormatMessage(IDS_OS_NOT_SUPPORTED,
                              display_name,
                              response_data.error_url());
    }
  } else if (_tcsicmp(kResponseStatusInternalError, status) == 0) {
    // "error-internal"
    info.status = COMPLETION_ERROR;
    info.error_code =
        static_cast<DWORD>(GOOPDATE_E_INTERNAL_ERROR_SERVER_RESPONSE);
    info.text.FormatMessage(IDS_NON_OK_RESPONSE_FROM_SERVER, status);
  } else {
    // Unknown response.
    info.status = COMPLETION_ERROR;
    info.error_code =
        static_cast<DWORD>(GOOPDATE_E_UNKNOWN_SERVER_RESPONSE);
    info.text.FormatMessage(IDS_NON_OK_RESPONSE_FROM_SERVER, status);
  }

  return info;
}

bool JobCreator::IsUpdateAvailable(const UpdateResponseData& response_data) {
  return _tcsicmp(kResponseStatusOkValue, response_data.status()) == 0;
}

// Check response and all of its children to see if any of them are in a state
// that requires an update job to be created.
bool JobCreator::IsUpdateAvailable(const UpdateResponse& response) {
  // If the top level response needs updating, short circuit here.
  if (IsUpdateAvailable(response.update_response_data())) {
    return true;
  }

  UpdateResponseDatas::const_iterator it;
  for (it = response.components_begin();
       it != response.components_begin();
       ++it) {
    if (IsUpdateAvailable(it->second)) {
      return true;
    }
  }

  return false;
}

}  // namespace omaha

