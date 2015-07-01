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

#include "omaha/plugins/update/site_lock.h"

#include <mshtml.h>
#include <shlobj.h>
#include <wininet.h>

#include "base/scoped_ptr.h"
#include "omaha/base/atl_regexp.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/reg_key.h"
#include "omaha/plugins/update/npapi/urlpropbag.h"

namespace omaha {

SiteLock::SiteLock() {
  for (int i = 0; i < arraysize(kSiteLockPatternStrings); ++i) {
    VERIFY1(AddPattern(kSiteLockPatternStrings[i]));
  }

  // TODO(omaha): should this be wrapped in a #ifdef DEBUG?
  CString dev_pattern_string;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueOneClickHostPattern,
                                 &dev_pattern_string)) &&
      !dev_pattern_string.IsEmpty()) {
    VERIFY1(AddPattern(dev_pattern_string));
  }
}

SiteLock::~SiteLock() {
  for (size_t i = 0; i < patterns_.size(); ++i) {
    delete patterns_[i];
  }
}

bool SiteLock::InApprovedDomain(IObjectWithSite* plugin) {
  CString url;
  if (FAILED(GetCurrentBrowserUrl(plugin, &url))) {
    return false;
  }
  return InApprovedDomain(url);
}

bool SiteLock::InApprovedDomain(const WCHAR* url) {
  // TODO(omaha): investigate using CUrl to remove dependency on wininet
  URL_COMPONENTS components = {sizeof(components)};
  components.dwHostNameLength = 1;
  if (!::InternetCrackUrl(url, 0, 0, &components)) {
    return false;
  }
  // On some platforms, InternetCrackUrl() is unreliable and will return
  // success but leave lpszHostName NULL.  Make sure it's valid.  (b/5532393)
  if (!components.lpszHostName || components.dwHostNameLength == 0) {
    return false;
  }
  CString hostname(components.lpszHostName, components.dwHostNameLength);
  for (std::vector<AtlRegExp*>::const_iterator it = patterns_.begin();
       it != patterns_.end();
       ++it) {
    AtlMatchContext context;
    if ((*it)->Match(hostname, &context)) {
      return true;
    }
  }

  return false;
}

HRESULT SiteLock::GetCurrentBrowserUrl(IObjectWithSite* plugin, CString* url) {
  if (SUCCEEDED(ExtractUrlFromBrowser(plugin, url))) {
    return S_OK;
  }
  if (SUCCEEDED(ExtractUrlFromPropBag(plugin, url))) {
    return S_OK;
  }
  return E_FAIL;
}

// TODO(omaha): Move this to common\webplugin_utils.
HRESULT SiteLock::GetUrlDomain(const CString& url, CString* url_domain) {
  ASSERT1(url_domain);
  url_domain->Empty();

  URL_COMPONENTS urlComponents = {0};
  urlComponents.dwStructSize = sizeof(urlComponents);
  urlComponents.dwSchemeLength = 1;
  urlComponents.dwHostNameLength = 1;
  if (!::InternetCrackUrl(url, 0, 0, &urlComponents)) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(L2, (_T("[InternetCrackUrl failed][0x%08x]"), hr));
    return hr;
  }

  CString scheme(urlComponents.lpszScheme, urlComponents.dwSchemeLength);
  CString host_name(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
  ASSERT1(!scheme.IsEmpty());
  ASSERT1(!host_name.IsEmpty());

  SafeCStringFormat(url_domain, _T("%s://%s/"), scheme, host_name);
  return S_OK;
}

bool SiteLock::AddPattern(const WCHAR* pattern) {
  ASSERT1(pattern);

  // An empty pattern will match everything...
  if (!*pattern) {
    ASSERT1(false);
    return false;
  }

  scoped_ptr<AtlRegExp> re(new AtlRegExp);
  REParseError error = re->Parse(pattern);
  if (REPARSE_ERROR_OK != error) {
    ASSERT(false, (L"Failed to parse site lock pattern: %s",
                   pattern));
    return false;
  }
  patterns_.push_back(re.release());
  return true;
}

// If the plugin is being hosted inside an NPAPI environment, NPUpdate will set
// a UrlPropertyBag object as our object site.  Fetch the URL used to create
// our object from it.
HRESULT SiteLock::ExtractUrlFromPropBag(IObjectWithSite* plugin, CString* url) {
  ASSERT1(plugin);
  ASSERT1(url);

  CComPtr<IPropertyBag> property_bag;
  HRESULT hr = plugin->GetSite(IID_PPV_ARGS(&property_bag));
  if (FAILED(hr)) {
    return hr;
  }

  CComVariant var;
  hr = property_bag->Read(kUrlPropertyBag_Url, &var, NULL);
  if (FAILED(hr)) {
    return hr;
  }
  if (var.vt != VT_BSTR || !var.bstrVal) {
    return E_UNEXPECTED;
  }
  *url = var.bstrVal;
  return S_OK;
}

// If the plugin is hosted in an ActiveX environment, IE will set itself as the
// object site.  Fetch the current URL from it.
HRESULT SiteLock::ExtractUrlFromBrowser(IObjectWithSite* plugin, CString* url) {
  ASSERT1(plugin);
  ASSERT1(url);

  CComPtr<IServiceProvider> service_provider;
  HRESULT hr = plugin->GetSite(IID_PPV_ARGS(&service_provider));
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IWebBrowser2> web_browser;
  hr = service_provider->QueryService(SID_SWebBrowserApp,
                                      IID_PPV_ARGS(&web_browser));

  CComBSTR bstr_url;
  if (SUCCEEDED(hr)) {
    hr = web_browser->get_LocationURL(&bstr_url);
  } else {
    // Do things the hard way...
    CComPtr<IOleClientSite> client_site;
    hr = plugin->GetSite(IID_PPV_ARGS(&client_site));
    if (FAILED(hr)) {
      return hr;
    }

    CComPtr<IOleContainer> container;
    hr = client_site->GetContainer(&container);
    if (FAILED(hr)) {
      return hr;
    }

    CComPtr<IHTMLDocument2> html_document;
    hr = container.QueryInterface(&html_document);
    if (FAILED(hr)) {
      return hr;
    }

    hr = html_document->get_URL(&bstr_url);
  }

  if (SUCCEEDED(hr)) {
    *url = bstr_url;
  }

  return hr;
}

}  // namespace omaha
