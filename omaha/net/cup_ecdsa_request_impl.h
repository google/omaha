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

#ifndef OMAHA_NET_CUP_ECDSA_REQUEST_IMPL_H__
#define OMAHA_NET_CUP_ECDSA_REQUEST_IMPL_H__

#include <atlstr.h>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "omaha/net/cup_ecdsa_utils.h"

namespace omaha {

namespace internal {

class CupEcdsaRequestImpl {
 public:
  explicit CupEcdsaRequestImpl(HttpRequestInterface* http_request);
  ~CupEcdsaRequestImpl();

  // Methods from HttpRequestInterface; will be forwarded from the outer
  // CupEcdsaRequest that is pimpl-ing to this object.

  HRESULT Close();
  HRESULT Send();
  HRESULT Cancel();
  HRESULT Pause();
  HRESULT Resume();
  std::vector<uint8> GetResponse() const;
  HRESULT QueryHeadersString(uint32 info_level,
                                    const TCHAR* name,
                                    CString* value) const;
  CString GetResponseHeaders() const;
  int GetHttpStatusCode() const;
  CString ToString() const;

  void set_session_handle(HINTERNET session_handle);
  void set_url(const CString& url);
  void set_request_buffer(const void* buffer, size_t buffer_length);
  void set_proxy_configuration(const ProxyConfig& proxy_config);
  void set_filename(const CString& filename);
  void set_low_priority(bool low_priority);
  void set_callback(NetworkRequestCallback* callback);
  void set_additional_headers(const CString& additional_headers);
  CString user_agent() const;
  void set_user_agent(const CString& user_agent);
  void set_proxy_auth_config(const ProxyAuthConfig& proxy_auth_config);

 private:
  friend class CupEcdsaRequestTest;

  HRESULT BuildRequest();
  HRESULT DoSend();
  HRESULT AuthenticateResponse();

  static bool ParseServerETag(const CString& etag_in,
                              EcdsaSignature* sig_out,
                              std::vector<uint8>* req_hash_out);

  // The transient state of the request, so that we can start always with a
  // clean slate even though the same instance is being reuse across requests.
  struct TransientCupState {
    TransientCupState();

    std::vector<uint8> request_hash;   // SHA-256 hash of the request body.

    CString cup2key;                   // Query parameter: key id and nonce
    CString cup2hreq;                  // Query parameter: request hash
    CString request_url;               // Complete URL of the request.

    std::vector<uint8> response;       // The received response body.
    CString etag;                      // The ETag header from the response.

    EcdsaSignature signature;          // The decoded ECDSA signature.
    std::vector<uint8> observed_hash;  // The observed hash of the request body.
  };
  std::unique_ptr<TransientCupState> cup_;

  CString     url_;                     // The original url.
  const void* request_buffer_;          // Contains the request body for POST.
  size_t      request_buffer_length_;   // Length of the request body.

  typedef const uint8 PublicKeyInstance[];
  typedef const uint8* PublicKey;

  EcdsaPublicKey public_key_;                      // Server public key.
  std::unique_ptr<HttpRequestInterface> http_request_;  // Inner http request.

  static const PublicKeyInstance kCupProductionPublicKey;
  static const PublicKeyInstance kCupTestPublicKey;

  DISALLOW_COPY_AND_ASSIGN(CupEcdsaRequestImpl);
};

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_NET_CUP_ECDSA_REQUEST_IMPL_H__
