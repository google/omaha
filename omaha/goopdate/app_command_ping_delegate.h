// Copyright 2013 Google Inc.
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

#ifndef OMAHA_GOOPDATE_APP_COMMAND_PING_DELEGATE_H__
#define OMAHA_GOOPDATE_APP_COMMAND_PING_DELEGATE_H__

#include "omaha/common/ping_event.h"
#include "omaha/goopdate/app_command_delegate.h"

namespace omaha {

class AppCommandPingDelegate : public AppCommandDelegate {
 public:
  AppCommandPingDelegate(const CString& app_guid,
                         bool is_machine,
                         const CString& session_id,
                         int reporting_id);
  virtual void OnLaunchResult(HRESULT result);
  virtual void OnObservationFailure(HRESULT result);
  virtual void OnCommandCompletion(DWORD exit_code);

 private:
  void SendPing(PingEvent::Types type,
                PingEvent::Results result,
                int error_code);

  CString app_guid_;
  bool is_machine_;
  CString session_id_;
  int reporting_id_;

  DISALLOW_COPY_AND_ASSIGN(AppCommandPingDelegate);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APP_COMMAND_PING_DELEGATE_H__
