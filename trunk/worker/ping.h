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

#ifndef OMAHA_WORKER_PING_H__
#define OMAHA_WORKER_PING_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace omaha {

class Job;
class NetworkRequest;
class Request;

class Ping {
 public:
  Ping();
  virtual ~Ping();

  // Sends a ping request. Returns an error if the request does not contain
  // any ping events.
  virtual HRESULT SendPing(Request* req);
  virtual HRESULT Cancel();

 private:
  HRESULT DoSendPing(const CString& url,
                     const CString& request_string);
  scoped_ptr<NetworkRequest> network_request_;
  DISALLOW_EVIL_CONSTRUCTORS(Ping);
};

}  // namespace omaha

#endif  // OMAHA_WORKER_PING_H__
