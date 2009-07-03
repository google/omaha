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
// BrowserRequest provides http transactions proxied through a user's browser.

#ifndef OMAHA_NET_BROWSER_REQUEST_H__
#define OMAHA_NET_BROWSER_REQUEST_H__

#include <list>
#include "omaha/net/urlmon_request.h"

namespace omaha {

typedef std::list<CAdapt<CComPtr<IBrowserHttpRequest2> > > BrowserObjects;

class BrowserRequest : public UrlmonRequest {
 public:
  BrowserRequest();
  virtual ~BrowserRequest() {}

  virtual CString ToString() const { return _T("iexplore"); }

  virtual HRESULT SendRequest(BSTR url,
                              BSTR post_data,
                              BSTR request_headers,
                              VARIANT response_headers_needed,
                              CComVariant* response_headers,
                              DWORD* response_code,
                              BSTR* cache_filename);

 private:
  bool GetAvailableBrowserObjects();
  BrowserObjects objects_;

  friend class BrowserRequestTest;
  friend class CupRequestTest;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserRequest);
};

}   // namespace omaha

#endif  // OMAHA_NET_BROWSER_REQUEST_H__

