// Copyright 2007-2010 Google Inc.
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

#ifndef OMAHA_NET_DETECTOR_H__
#define OMAHA_NET_DETECTOR_H__

#include <atlstr.h>
#include <windows.h>
#include <memory>

#include "base/basictypes.h"

namespace omaha {

struct ProxyConfig;

class ProxyDetectorInterface {
 public:
  // Detects proxy information.
  virtual HRESULT Detect(ProxyConfig* config) = 0;
  virtual const TCHAR* source() = 0;
  virtual ~ProxyDetectorInterface() {}
};

// Detects proxy override information in the specified registry key.
class RegistryOverrideProxyDetector : public ProxyDetectorInterface {
 public:
  explicit RegistryOverrideProxyDetector(const CString& reg_path)
      : reg_path_(reg_path) {}

  HRESULT Detect(ProxyConfig* config) override;
  const TCHAR* source() override { return _T("RegistryOverride"); }
 private:
  CString reg_path_;
  DISALLOW_COPY_AND_ASSIGN(RegistryOverrideProxyDetector);
};

class UpdateDevProxyDetector : public ProxyDetectorInterface {
 public:
  UpdateDevProxyDetector();
  HRESULT Detect(ProxyConfig* config) override {
    return registry_detector_.Detect(config);
  }
  const TCHAR* source() override { return _T("UpdateDev"); }
 private:
  RegistryOverrideProxyDetector registry_detector_;
  DISALLOW_COPY_AND_ASSIGN(UpdateDevProxyDetector);
};

// A version that picks up proxy override from Enterprise Policy.
class PolicyProxyDetector : public ProxyDetectorInterface {
 public:
  PolicyProxyDetector() {}
  HRESULT Detect(ProxyConfig* config) override;
  const TCHAR* source() override { return _T("Policy"); }

 private:
  HRESULT GetProxyMode(CString* proxy_mode);
  HRESULT GetProxyPacUrl(CString* proxy_pac_url);
  HRESULT GetProxyServer(CString* proxy_server);

  DISALLOW_COPY_AND_ASSIGN(PolicyProxyDetector);
};

// Detects winhttp proxy information. This is what the winhttp proxy
// configuration utility (proxycfg.exe) has set.
// The winhttp proxy settings are under:
// HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings\Connections
class DefaultProxyDetector : public ProxyDetectorInterface {
 public:
  DefaultProxyDetector() {}
  HRESULT Detect(ProxyConfig* config) override;
  const TCHAR* source() override { return _T("winhttp"); }
 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultProxyDetector);
};

namespace internal {

// Detects wininet proxy information for the current user. The caller must
// run as user to retrieve the correct information.
// It works only when the calling code runs as or it impersonates a user.
class IEProxyDetector : public ProxyDetectorInterface {
 public:
  IEProxyDetector() {}
  HRESULT Detect(ProxyConfig* config) override;
  const TCHAR* source() override { return _T("IE"); }
 private:
  DISALLOW_COPY_AND_ASSIGN(IEProxyDetector);
};

}  // namespace internal

// Detects wininet WPAD proxy information for the current user.
// It works only when the calling code runs as or impersonates a user.
class IEWPADProxyDetector : public internal::IEProxyDetector {
 public:
  IEWPADProxyDetector() {}
  HRESULT Detect(ProxyConfig* config) override;
  const TCHAR* source() override { return _T("IEWPAD"); }

 private:
  DISALLOW_COPY_AND_ASSIGN(IEWPADProxyDetector);
};

// Detects wininet PAC proxy information for the current user.
// It works only when the calling code runs as or impersonates a user.
class IEPACProxyDetector : public internal::IEProxyDetector {
 public:
  IEPACProxyDetector() {}
  HRESULT Detect(ProxyConfig* config) override;
  const TCHAR* source() override { return _T("IEPAC"); }

 private:
  DISALLOW_COPY_AND_ASSIGN(IEPACProxyDetector);
};

// Detects wininet Named proxy information for the current user.
// It works only when the calling code runs as or impersonates a user.
class IENamedProxyDetector : public internal::IEProxyDetector {
 public:
  IENamedProxyDetector() {}
  HRESULT Detect(ProxyConfig* config) override;
  const TCHAR* source() override { return _T("IENamed"); }

 private:
  DISALLOW_COPY_AND_ASSIGN(IENamedProxyDetector);
};

}  // namespace omaha

#endif  // OMAHA_NET_DETECTOR_H__

