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
// TODO(omaha): to minimize conversions from UNICODE to UTF-8 consider an
// API that takes the request body as UTF-8 and it stores it as UTF-8.

#include "omaha/net/cup_request.h"

#include <atlconv.h>
#include <atlstr.h>
#include <vector>
#include "omaha/common/const_addresses.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/encrypt.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/security/b64.h"
#include "omaha/common/security/hmac.h"
#include "omaha/common/security/rsa.h"
#include "omaha/common/security/sha.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/net/cup_utils.h"
#include "omaha/net/http_client.h"
#include "omaha/net/net_utils.h"
#include "omaha/net/network_config.h"

using omaha::encrypt::EncryptData;
using omaha::encrypt::DecryptData;

namespace omaha {

namespace detail {

class CupRequestImpl {
 public:
  explicit CupRequestImpl(HttpRequestInterface* http_request);
  ~CupRequestImpl();

  HRESULT Close();
  HRESULT Send();
  HRESULT Cancel();
  std::vector<uint8> GetResponse() const;
  HRESULT QueryHeadersString(uint32 info_level,
                                    const TCHAR* name,
                                    CString* value) const;
  CString GetResponseHeaders() const;
  int GetHttpStatusCode() const;
  CString ToString() const;
  void SetEntropy(const void* data, size_t data_length);

  void set_session_handle(HINTERNET session_handle);
  void set_url(const CString& url);
  void set_request_buffer(const void* buffer, size_t buffer_length);
  void set_network_configuration(const Config& network_config);
  void set_filename(const CString& filename);
  void set_low_priority(bool low_priority);
  void set_callback(NetworkRequestCallback* callback);
  void set_additional_headers(const CString& additional_headers);
  CString user_agent() const;
  void set_user_agent(const CString& user_agent);

 private:
  HRESULT DoSend();
  HRESULT BuildRequest();
  HRESULT BuildChallengeHash();
  HRESULT BuildClientProof();
  HRESULT InitializeEntropy();
  HRESULT AuthenticateResponse();

  // Loads the {sk, c} credentials from persistent storage.
  HRESULT LoadCredentials(std::vector<uint8>* sk, CStringA* c);

  // Saves the {sk, c} credentials. The key is encrypted before saving it.
  HRESULT SaveCredentials(const std::vector<uint8>& sk, const CStringA& c);

  // Replaces the https protocol scheme with http.
  CString BuildInnerRequestUrl(const CString& url);

  // The transient state of the request, so that we can start always with a
  // clean slate even though the same instance is being reuse across requests.
  struct TransientCupState {
    std::vector<uint8> entropy;
    std::vector<uint8> r;         // Random bytes (r).
    std::vector<uint8> sk;        // Cached shared key (sk)
    std::vector<uint8> new_sk;    // Current shared_key (sk').
    std::vector<uint8> hw;        // Challenge hash (hw).
    std::vector<uint8> hm;        // Response hash (hm).

    CStringA cp;                  // Client proof (cp).
    CStringA sp;                  // Server proof (sp).
    CStringA vw;                  // Versioned challenge (v|w).
    std::vector<uint8> response;  // The received response.
    CStringA c;                   // Cached client cookie (c)
    CStringA new_cookie;          // The cookie returned by the server (c').

    // The url of the request. It includes the original url plus the versioned
    // challenge (v|w).
    CStringA request_url;
  };
  scoped_ptr<TransientCupState> cup_;

  CStringA url_;                        // The original url.
  const void* request_buffer_;          // Contains the request body for POST.
  size_t      request_buffer_length_;   // Length of the request body.
  std::vector<uint8> entropy_;          // Optional entropy pass by the caller,
                                        // mostly for testing purpose.

  // {sk, c} credentials. They are persisted in the registry when the object is
  // destroyed. The vast majority of network code runs impersonated. Writing
  // through the credentials is likely to fail. However, due to how the
  // CUP object is being used by the upper layers of the network code, a
  // write back policy is possible.
  std::vector<uint8> persisted_sk_;
  CStringA           persisted_c_;

  scoped_ptr<RSA> rsa_;
  RSA::PublicKey public_key_;                      // Server public key (pk[v]).
  scoped_ptr<HttpRequestInterface> http_request_;  // Inner http request.

  static const RSA::PublicKeyInstance kCupProductionPublicKey;
  static const RSA::PublicKeyInstance kCupTestPublicKey;

  DISALLOW_EVIL_CONSTRUCTORS(CupRequestImpl);
};

const RSA::PublicKeyInstance CupRequestImpl::kCupProductionPublicKey =
#include "omaha/net/cup_pubkey.3.h"
;   // NOLINT

const RSA::PublicKeyInstance CupRequestImpl::kCupTestPublicKey =
#include "omaha/net/cup_pubkey.2.h"
;   // NOLINT

CupRequestImpl::CupRequestImpl(HttpRequestInterface* http_request)
    : request_buffer_(NULL),
      request_buffer_length_(0),
      public_key_(NULL) {
  ASSERT1(http_request);
  bool is_using_cup_test_keys = NetworkConfig::IsUsingCupTestKeys();
  public_key_ = is_using_cup_test_keys ? kCupTestPublicKey :
                                         kCupProductionPublicKey;

  // Try to retrieve the credentials if we have any. If we have succeeded, then
  // we must have a {sk, c} pair. If we have failed, then we will generate
  // a fresh set of credentials later on.
  HRESULT hr = LoadCredentials(&persisted_sk_, &persisted_c_);
  if (FAILED(hr)) {
    ASSERT1(persisted_sk_.empty());
    ASSERT1(persisted_c_.IsEmpty());
  }

  rsa_.reset(new RSA(public_key_));
  http_request_.reset(http_request);

  // Decorate the user agent by appending the "CUP" suffix. This overrides
  // the user agent of the inner http request.
  CString user_agent(http_request_->user_agent());
  user_agent += _T(";cup");
  http_request_->set_user_agent(user_agent);
}

CupRequestImpl::~CupRequestImpl() {
  Close();
}

HRESULT CupRequestImpl::Close() {
  cup_.reset();

  // TODO(omaha): optimize so that if the credentials did not change then
  // there would be not need to write back.
  if (!persisted_sk_.empty() && !persisted_c_.IsEmpty()) {
    VERIFY1(SUCCEEDED(SaveCredentials(persisted_sk_, persisted_c_)));
  }
  return http_request_->Close();
}

HRESULT CupRequestImpl::Send() {
  // Start with a fresh CUP state. This is important as the client may
  // reuse the same CUP request for subsequent requests.
  cup_.reset(new TransientCupState);

  // First, build a request, send it, and then authenticate the response.
  HRESULT hr = BuildRequest();
  if (FAILED(hr)) {
    return hr;
  }
  hr = DoSend();
  if (FAILED(hr)) {
    return hr;
  }
  hr = AuthenticateResponse();
  if (FAILED(hr)) {
    return hr;
  }
  return S_OK;
}

HRESULT CupRequestImpl::BuildRequest() {
  HRESULT hr(InitializeEntropy());
  if (FAILED(hr)) {
    return hr;
  }
  // Start with some random bytes.
  cup_->r = cup_utils::RsaPad(rsa_->size(),
                              &cup_->entropy.front(), cup_->entropy.size());

  // Derive a new shared key (sk') as the hash of the random bytes.
  // TODO(omaha): consider protecting the key using ::CryptProtectmemory when
  // not being used.
  cup_->new_sk = cup_utils::Hash(cup_->r);

  // Compute the challenge (w) by encrypting in place (r) with the server
  // public key pk[v].
  size_t encrypted_size = rsa_->raw(&cup_->r.front(), cup_->r.size());
  ASSERT1(encrypted_size == cup_->r.size());

  // Compute the versioned challenge (v|w) as
  // decimal-v:base64-encoded-rsa-wrapper.
  cup_->vw.Format("%d:%s",
                  rsa_->version(),
                  cup_utils::B64Encode(&cup_->r.front(), cup_->r.size()));

  // Compute the url of the CUP request.
  // Append a query string or append to the existing query string if any.
  const char* format_string = url_.Find('?') != -1 ? "%1&w=%2" : "%1?w=%2";
  cup_->request_url.FormatMessage(format_string, url_, cup_->vw);

  // Compute the challenge hash (hw) as HASH(HASH(v|w)|HASH(req))
  hr = BuildChallengeHash();
  if (FAILED(hr)) {
    return hr;
  }

  // Compute the client proof as SYMsign[sk](0|hw|HASH(c)) if we have a
  // {sk, c} pair or as SYMsign[sk'](3|hw) if we do not.
  hr = BuildClientProof();
  if (FAILED(hr)) {
    return hr;
  }

  NET_LOG(L4, (_T("[hw: %s]"), BytesToHex(cup_->hw)));

  // This is what the client sends up along with the request: a versioned
  // challenge, a client proof, and a client cookie if it has one.

  NET_LOG(L4, (_T("[request:     %s]"), CA2T(cup_->request_url)));
  NET_LOG(L4, (_T("[ifmatch:     %s]"), CA2T(cup_->cp)));
  NET_LOG(L4, (_T("[cookie:      %s]"), CA2T(cup_->c)));

  return S_OK;
}

HRESULT CupRequestImpl::BuildChallengeHash() {
  // The hash of the request if carried through all the cryptographic proofs.
  // The challenge hash hw is computed as:
  // HASH(HASH(v|w)|HASH(request_url)|HASH(body)).
  // For simplicity, the protocol scheme and the port are dropped before
  // hashing. The hash is computed over the CUP request url, which includes
  // the versioned hash.
  scoped_ptr<HttpClient> http_client(CreateHttpClient());
  if (!http_client.get()) {
    return OMAHA_NET_E_CUP_NO_HTTP_CLIENT;
  }
  HRESULT hr = http_client->Initialize();
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(!cup_->request_url.IsEmpty());
  CString url(cup_->request_url);
  CString scheme, server, url_path, query_string;
  hr = http_client->CrackUrl(url,
                             0,
                             &scheme,
                             &server,
                             NULL,
                             &url_path,
                             &query_string);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(!scheme.IsEmpty());
  ASSERT1(!server.IsEmpty());
  ASSERT1(!url_path.IsEmpty());
  url.FormatMessage(_T("//%1%2%3"), server, url_path, query_string);

  CStringA req(url);

  // Compute hw as HASH(HASH(v|w)|HASH(request_url)|HASH(body)).
  ASSERT1(!cup_->vw.IsEmpty());
  ASSERT1(!url.IsEmpty());
  cup_->hw = cup_utils::HashBuffers(cup_->vw.GetString(), cup_->vw.GetLength(),
                                    req.GetString(), req.GetLength(),
                                    request_buffer_, request_buffer_length_);
  return S_OK;
}

HRESULT CupRequestImpl::BuildClientProof() {
  // Use persisted {sk, c} or start with empty credentials otherwise.
  ASSERT1(cup_->sk.empty());
  ASSERT1(cup_->c.IsEmpty());
  if (!persisted_sk_.empty() && !persisted_c_.IsEmpty()) {
    cup_->sk = persisted_sk_;
    cup_->c  = persisted_c_;
  }

  std::vector<uint8> hcp;   // hmac of the client proof.
  if (!cup_->sk.empty() && !cup_->c.IsEmpty()) {
    // Use the cached shared key (sk) and the cookie (c) if we have them.
    ASSERT1(cup_->sk.size() == SHA_DIGEST_SIZE);
    NET_LOG(L4, (_T("[using sk: %s]"), BytesToHex(cup_->sk)));

    // Compute 'cp' as SYMsign[sk](0|HASH(w)|HASH(c))
    std::vector<uint8> hc = cup_utils::Hash(cup_->c);
    hcp = cup_utils::SymSign(cup_->sk, 0, &cup_->hw, &hc, NULL);
  } else {
    // There is no saved shared key. Use current shared key (new_sk).
    ASSERT1(cup_->new_sk.size() == SHA_DIGEST_SIZE);
    NET_LOG(L4, (_T("[using sk': %s]"), BytesToHex(cup_->new_sk)));

    // Compute 'cp' as SYMsign[sk'](3|HASH(w))
    hcp = cup_utils::SymSign(cup_->new_sk, 3, &cup_->hw, NULL, NULL);
  }

  NET_LOG(L4, (_T("[client proof hmac: %s]"), BytesToHex(hcp)));
  cup_->cp = cup_utils::B64Encode(hcp);
  return S_OK;
}

HRESULT CupRequestImpl::AuthenticateResponse() {
  // This is what the server has sent down along with the response:
  // a server proof, and optionally a new cookie for the client.
  NET_LOG(L4, (_T("[etag:        %s]"), CA2T(cup_->sp)));
  NET_LOG(L4, (_T("[svr cookie:  %s]"), CA2T(cup_->new_cookie)));

  if (cup_->sp.IsEmpty()) {
    // We can't authenticate anything without the server proof.
    return OMAHA_NET_E_CUP_NO_SERVER_PROOF;
  }

  // Compute the hash of the response (hm).
  cup_->hm = cup_utils::Hash(cup_->response);
  NET_LOG(L4, (_T("[hm: %s]"), BytesToHex(cup_->hm)));

  // Verify the response and the challenge w are authenticated by the
  // client shared key. The validation has at least one subtle aspect: the
  // client must try two signatures. First, it tries to authenticate using
  // the new sk and the new cookie. If the response does not authenticate but
  // the client has an old key, it should try authenticating with the old key.
  // This second step is important in the case of an attacker or server
  // misconfiguration where the server is signing with the old key but still
  // sending a cookie.
  std::vector<uint8> hmac;
  if (!cup_->new_cookie.IsEmpty() && !cup_->new_sk.empty()) {
    // The server has sent down a new cookie because the client proof or the
    // client cookie were not good or missing. Try to authenticate using the
    // new shared key and the new cookie {sk', c'}.
    std::vector<uint8> hnew_c = cup_utils::Hash(cup_->new_cookie);  // HASH(c')

    // Compute the server proof (sp) as SYMsign[sk'](1|hw|hm|hc').
    hmac = cup_utils::SymSign(cup_->new_sk, 1, &cup_->hw, &cup_->hm, &hnew_c);
    CStringA expected_sp = cup_utils::B64Encode(hmac);

    if (expected_sp == cup_->sp) {
      // Copy the credentials to write them back when this object is destroyed.
      persisted_sk_ = cup_->new_sk;
      persisted_c_  = cup_->new_cookie;
      return S_OK;
    }
  }

  if (!cup_->sk.empty()) {
    // The server has accepted our client proof (cp) and consequently the sk.

    // Compute the server proof as SYMsign[sk](2|hw|hm).
    hmac = cup_utils::SymSign(cup_->sk, 2, &cup_->hw, &cup_->hm, NULL);
    CStringA expected_sp = cup_utils::B64Encode(hmac);

    if (expected_sp == cup_->sp) {
      return S_OK;
    }
  }

  NET_LOG(L4, (_T("[expected server proof: %s]"), BytesToHex(hmac)));

  // The server proof does not authenticate. We reject this response.
  return OMAHA_NET_E_CUP_NOT_TRUSTED;
}

HRESULT CupRequestImpl::LoadCredentials(std::vector<uint8>* sk, CStringA* c) {
  ASSERT1(sk);
  ASSERT1(c);
  NetworkConfig& network_config = NetworkConfig::Instance();
  CupCredentials cup_credentials;
  HRESULT hr = network_config.GetCupCredentials(&cup_credentials);
  if (FAILED(hr)) {
    return hr;
  }
  std::vector<uint8> decrypted_sk;
  hr = DecryptData(NULL,
                   0,
                   &cup_credentials.sk.front(),
                   cup_credentials.sk.size(),
                   &decrypted_sk);
  if (FAILED(hr)) {
    return hr;
  }
  sk->swap(decrypted_sk);
  c->SetString(cup_credentials.c);
  return S_OK;
}

HRESULT CupRequestImpl::SaveCredentials(const std::vector<uint8>& sk,
                                        const CStringA& c) {
  // Update the client persistent state if the response has been validated.
  // The client key is encrypted while on the disk.
  NetworkConfig& network_config = NetworkConfig::Instance();
  CupCredentials cup_credentials;
  cup_credentials.c.SetString(c);
  HRESULT hr = EncryptData(NULL,
                           0,
                           &sk.front(),
                           sk.size(),
                           &cup_credentials.sk);
  if (FAILED(hr)) {
    return hr;
  }
  // Try to persist the new client credentials.
  hr = network_config.SetCupCredentials(&cup_credentials);
  if (FAILED(hr)) {
    return hr;
  }
  return S_OK;
}

HRESULT CupRequestImpl::DoSend() {
  // The url of the inner request includes the versioned challenge.
  // It replaces the protocol from https to http since CUP over https is
  // not supported.
  http_request_->set_url(BuildInnerRequestUrl(CString(cup_->request_url)));

  // Prepare additional headers to send.
  CString additional_headers;

  // The client proof (cp) is sent as the "If-Match" header.
  ASSERT1(!cup_->cp.IsEmpty());
  CStringA if_match_header;
  if_match_header.Format("\"%s\"", cup_->cp);
  additional_headers += HttpClient::BuildRequestHeader(_T("If-Match"),
                                                       CA2T(if_match_header));

  // Send the client cookie (c) if we have one.
  if (!cup_->c.IsEmpty()) {
    additional_headers += HttpClient::BuildRequestHeader(_T("Cookie"),
                                                         CA2T(cup_->c));
  }
  http_request_->set_additional_headers(additional_headers);

  NET_LOG(L5, (_T("[CUP request][%s]"),
               BufferToPrintableString(request_buffer_,
                                       request_buffer_length_)));
  HRESULT hr = http_request_->Send();
  if (FAILED(hr)) {
    return hr;
  }
  http_request_->GetResponse().swap(cup_->response);
  int status_code(http_request_->GetHttpStatusCode());
  if (status_code != HTTP_STATUS_OK) {
    return HRESULTFromHttpStatusCode(status_code);
  }
  NET_LOG(L5, (_T("[CUP response][%s]"),
               VectorToPrintableString(cup_->response)));

  // Get the server proof (sp).
  CString etag_header;
  http_request_->QueryHeadersString(WINHTTP_QUERY_ETAG, NULL, &etag_header);
  UnenclosePath(&etag_header);   // Remove the quotes.
  cup_->sp = CT2A(etag_header);

  // Get the client cookie c'. The cookie may or may not be there. If the
  // server has accepted both client proof (sp) then no
  // new cookie is sent down. This is an indication for the client to
  // use its {sk, c} pair.
  CString set_cookie_header;
  http_request_->QueryHeadersString(WINHTTP_QUERY_SET_COOKIE,
                                    NULL, &set_cookie_header);

  // Parse the cookie header to extract c=xxx cookie.
  cup_->new_cookie = CT2A(cup_utils::ParseCupCookie(set_cookie_header));
  return S_OK;
}

HRESULT CupRequestImpl::Cancel() {
  return http_request_->Cancel();
}

std::vector<uint8> CupRequestImpl::GetResponse() const {
  return http_request_->GetResponse();
}

HRESULT CupRequestImpl::QueryHeadersString(uint32 info_level,
                                           const TCHAR* name,
                                           CString* value) const {
  return http_request_->QueryHeadersString(info_level, name, value);
}

CString CupRequestImpl::GetResponseHeaders() const {
  return http_request_->GetResponseHeaders();
}

int CupRequestImpl::GetHttpStatusCode() const {
  return http_request_->GetHttpStatusCode();
}

CString CupRequestImpl::ToString() const {
  return CString("CUP:") + http_request_->ToString();
}

void CupRequestImpl::set_session_handle(HINTERNET session_handle) {
  http_request_->set_session_handle(session_handle);
}

void CupRequestImpl::set_url(const CString& url) {
  url_ = CT2A(url, CP_UTF8);
}

void CupRequestImpl::set_request_buffer(const void* buffer,
                                        size_t buffer_length) {
  request_buffer_ = buffer;
  request_buffer_length_ = buffer_length;
  http_request_->set_request_buffer(request_buffer_, request_buffer_length_);
}

void CupRequestImpl::set_network_configuration(const Config& network_config) {
  http_request_->set_network_configuration(network_config);
}

void CupRequestImpl::set_filename(const CString& filename) {
  http_request_->set_filename(filename);
}

void CupRequestImpl::set_low_priority(bool low_priority) {
  http_request_->set_low_priority(low_priority);
}

void CupRequestImpl::set_callback(NetworkRequestCallback* callback) {
  http_request_->set_callback(callback);
}

void CupRequestImpl::set_additional_headers(const CString& additional_headers) {
  http_request_->set_additional_headers(additional_headers);
}

CString CupRequestImpl::user_agent() const {
  return http_request_->user_agent();
}

void CupRequestImpl::set_user_agent(const CString& user_agent) {
  http_request_->set_user_agent(user_agent);
}

void CupRequestImpl::SetEntropy(const void* data, size_t data_length) {
  ASSERT(false, (_T("Do not call from production code")));
  ASSERT1(data);
  const uint8* first(static_cast<const uint8*>(data));
  const uint8* last(first + data_length);
  std::vector<uint8> new_entropy(first, last);
  entropy_.swap(new_entropy);
}

HRESULT CupRequestImpl::InitializeEntropy() {
  if (entropy_.empty()) {
    cup_->entropy.resize(rsa_->size() - SHA_DIGEST_SIZE);
    if (!GenRandom(&cup_->entropy.front(), cup_->entropy.size())) {
      return OMAHA_NET_E_CUP_NO_ENTROPY;
    }
  } else {
    cup_->entropy = entropy_;
  }
  return S_OK;
}

CString CupRequestImpl::BuildInnerRequestUrl(const CString& url) {
  CString result(url);
  if (String_StartsWith(result, kHttpsProtoScheme, true)) {
    result = url.Mid(_tcslen(kHttpsProtoScheme));
    result.Insert(0, kHttpProtoScheme);
  }
  return result;
}

}   // namespace detail


CupRequest::CupRequest(HttpRequestInterface* http_request) {
  ASSERT1(http_request);
  impl_.reset(new detail::CupRequestImpl(http_request));
}

CupRequest::~CupRequest() {
}

HRESULT CupRequest::Close() {
  return impl_->Close();
}

HRESULT CupRequest::Send() {
  return impl_->Send();
}

HRESULT CupRequest::Cancel() {
  return impl_->Cancel();
}

std::vector<uint8> CupRequest::GetResponse() const {
  return impl_->GetResponse();
}

int CupRequest::GetHttpStatusCode() const {
  return impl_->GetHttpStatusCode();
}

HRESULT CupRequest::QueryHeadersString(uint32 info_level,
                                       const TCHAR* name,
                                       CString* value) const {
  return impl_->QueryHeadersString(info_level, name, value);
}

CString CupRequest::GetResponseHeaders() const {
  return impl_->GetResponseHeaders();
}

CString CupRequest::ToString() const {
  return impl_->ToString();
}

void CupRequest::set_session_handle(HINTERNET session_handle) {
  return impl_->set_session_handle(session_handle);
}

void CupRequest::set_url(const CString& url) {
  impl_->set_url(url);
}

void CupRequest::set_request_buffer(const void* buffer, size_t buffer_length) {
  impl_->set_request_buffer(buffer, buffer_length);
}

void CupRequest::set_network_configuration(const Config& network_config) {
  impl_->set_network_configuration(network_config);
}

void CupRequest::set_filename(const CString& filename) {
  impl_->set_filename(filename);
}

void CupRequest::set_low_priority(bool low_priority) {
  impl_->set_low_priority(low_priority);
}

void CupRequest::set_callback(NetworkRequestCallback* callback) {
  impl_->set_callback(callback);
}

void CupRequest::set_additional_headers(const CString& additional_headers) {
  impl_->set_additional_headers(additional_headers);
}

CString CupRequest::user_agent() const {
  return impl_->user_agent();
}

void CupRequest::set_user_agent(const CString& user_agent) {
  impl_->set_user_agent(user_agent);
}

void CupRequest::SetEntropy(const void* data, size_t data_length) {
  return impl_->SetEntropy(data, data_length);
}

}   // namespace omaha

