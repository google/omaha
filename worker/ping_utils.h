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

#ifndef OMAHA_WORKER_PING_UTILS_H__
#define OMAHA_WORKER_PING_UTILS_H__

#include <atlstr.h>
#include <windows.h>
#include "omaha/worker/ping_event.h"
#include "omaha/worker/product_data.h"

namespace omaha {

class AppRequestData;
class Ping;
class Request;
enum JobCompletionStatus;
struct CommandLineExtraArgs;
struct CompletionInfo;

// Utility functions for Ping.
namespace ping_utils {

// TODO(omaha): Put common params in the same order.
HRESULT SendGoopdatePing(bool is_machine,
                         const CommandLineExtraArgs& extra_args,
                         const CString& install_source,
                         PingEvent::Types type,
                         HRESULT error,
                         int extra_code1,
                         const TCHAR* version,
                         Ping* ping);

HRESULT SendPostSetupPing(HRESULT result,
                          int extra_code1,
                          const CString& previous_version,
                          bool is_machine,
                          bool is_interactive,
                          const CommandLineExtraArgs& extra,
                          const CString& install_source,
                          Ping* ping);

PingEvent::Results CompletionStatusToPingEventResult(
    JobCompletionStatus status);

HRESULT BuildCompletedPingForAllProducts(const ProductDataVector& products,
                                         bool is_update,
                                         const CompletionInfo& info,
                                         Request* request);

HRESULT SendCompletedPingsForAllProducts(const ProductDataVector& products,
                                         bool is_machine,
                                         bool is_update,
                                         const CompletionInfo& info,
                                         Ping* ping);

void BuildGoogleUpdateAppRequestData(bool is_machine,
                                     const CommandLineExtraArgs& extra_args,
                                     const CString& install_source,
                                     CString* previous_version,
                                     AppRequestData* app_request_data);

}  // namespace ping_utils

}  // namespace omaha

#endif  // OMAHA_WORKER_PING_UTILS_H__
