// Copyright 2009 Google Inc.
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

#ifndef OMAHA_PLUGINS_UPDATE_SITE_LOCK_H_
#define OMAHA_PLUGINS_UPDATE_SITE_LOCK_H_

#include <windows.h>
#include <tchar.h>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/atl_regexp.h"

namespace omaha {

class SiteLock {
 public:
  SiteLock();
  ~SiteLock();

  bool InApprovedDomain(IObjectWithSite* url_provider);
  bool InApprovedDomain(const WCHAR* url);

  static HRESULT GetCurrentBrowserUrl(IObjectWithSite* plugin, CString* url);
  static HRESULT GetUrlDomain(const CString& url, CString* url_domain);

 private:
  bool AddPattern(const WCHAR* pattern);

  static HRESULT ExtractUrlFromBrowser(IObjectWithSite* plugin, CString* url);
  static HRESULT ExtractUrlFromPropBag(IObjectWithSite* plugin, CString* url);

  std::vector<AtlRegExp*> patterns_;

  DISALLOW_COPY_AND_ASSIGN(SiteLock);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_SITE_LOCK_H_
