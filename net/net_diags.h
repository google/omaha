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


#ifndef OMAHA_NET_NET_DIAGS_H__
#define OMAHA_NET_NET_DIAGS_H__

#include <tchar.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/net/network_request.h"

namespace omaha {

class NetDiags : public NetworkRequestCallback {
 public:
  NetDiags();
  ~NetDiags();

  // Run the tests.
  int Main();

 private:
  void Initialize();
  virtual void OnProgress(int bytes, int bytes_total, int, const TCHAR*);

  // http get.
  void DoGet(const CString& url);
  void DoDownload(const CString& url);
};

}   // namespace omaha

#endif  // OMAHA_NET_NET_DIAGS_H__

