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

#include "omaha/net/cup_ecdsa_request.h"
#include "omaha/net/cup_ecdsa_request_impl.h"

#include <atlconv.h>
#include <atlstr.h>
#include <algorithm>
#include <limits>
#include <vector>

#include "base/rand_util.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/security/p256.h"
#include "omaha/base/security/sha256.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/net/cup_ecdsa_metrics.h"
#include "omaha/net/cup_ecdsa_utils.h"
#include "omaha/net/http_client.h"
#include "omaha/net/net_utils.h"
#include "omaha/net/network_config.h"

namespace omaha {

namespace internal {

const uint8 CupEcdsaRequestImpl::kCupProductionPublicKey[] =
#include "omaha/net/cup_ecdsa_pubkey.14.h"
    ;  // NOLINT

const uint8 CupEcdsaRequestImpl::kCupTestPublicKey[] =
#include "omaha/net/cup_ecdsa_pubkey.3.h"
;   // NOLINT

CupEcdsaRequestImpl::CupEcdsaRequestImpl(HttpRequestInterface* http_request)
    : request_buffer_(NULL),
      request_buffer_length_(0) {
  ASSERT1(http_request);

  // Load the appropriate ECC public key.
  const uint8* const encoded_public_key =
      NetworkConfig::IsUsingCupTestKeys() ? kCupTestPublicKey :
                                            kCupProductionPublicKey;
  public_key_.DecodeFromBuffer(encoded_public_key);

  // Store the inner HTTP request.
  http_request_.reset(http_request);

  // Decorate the user agent by appending a CUP-ECDSA suffix. (This overrides
  // the user agent of the inner HTTP request.)
  CString user_agent(http_request_->user_agent());
  user_agent += _T(";cup-ecdsa");
  http_request_->set_user_agent(user_agent);
}

CupEcdsaRequestImpl::~CupEcdsaRequestImpl() {
  Close();
}

HRESULT CupEcdsaRequestImpl::Close() {
  cup_.reset();
  return http_request_->Close();
}

HRESULT CupEcdsaRequestImpl::Send() {
  metric_cup_ecdsa_total++;

  cup_.reset(new TransientCupState);

  HRESULT hr = BuildRequest();
  if (FAILED(hr)) {
    return hr;
  }

  hr = DoSend();
  if (FAILED(hr)) {
    return hr;
  }

  return AuthenticateResponse();
}

HRESULT CupEcdsaRequestImpl::Cancel() {
  return http_request_->Cancel();
}

HRESULT CupEcdsaRequestImpl::Pause() {
  return http_request_->Pause();
}

HRESULT CupEcdsaRequestImpl::Resume() {
  return http_request_->Resume();
}

std::vector<uint8> CupEcdsaRequestImpl::GetResponse() const {
  return http_request_->GetResponse();
}

HRESULT CupEcdsaRequestImpl::QueryHeadersString(uint32 info_level,
                                                const TCHAR* name,
                                                CString* value) const {
  return http_request_->QueryHeadersString(info_level, name, value);
}

CString CupEcdsaRequestImpl::GetResponseHeaders() const {
  return http_request_->GetResponseHeaders();
}

int CupEcdsaRequestImpl::GetHttpStatusCode() const {
  return http_request_->GetHttpStatusCode();
}

CString CupEcdsaRequestImpl::ToString() const {
  return CString("CUP-ECDSA:") + http_request_->ToString();
}

void CupEcdsaRequestImpl::set_session_handle(HINTERNET session_handle) {
  http_request_->set_session_handle(session_handle);
}

void CupEcdsaRequestImpl::set_url(const CString& url) {
  url_ = url;
}

void CupEcdsaRequestImpl::set_request_buffer(const void* buffer,
                                             size_t buffer_length) {
  request_buffer_ = buffer;
  request_buffer_length_ = buffer_length;
  http_request_->set_request_buffer(request_buffer_, request_buffer_length_);
}

void CupEcdsaRequestImpl::set_proxy_configuration(
    const ProxyConfig& proxy_config) {
  http_request_->set_proxy_configuration(proxy_config);
}

void CupEcdsaRequestImpl::set_filename(const CString& filename) {
  http_request_->set_filename(filename);
}

void CupEcdsaRequestImpl::set_low_priority(bool low_priority) {
  http_request_->set_low_priority(low_priority);
}

void CupEcdsaRequestImpl::set_callback(NetworkRequestCallback* callback) {
  http_request_->set_callback(callback);
}

void CupEcdsaRequestImpl::set_additional_headers(
    const CString& additional_headers) {
  http_request_->set_additional_headers(additional_headers);
}

CString CupEcdsaRequestImpl::user_agent() const {
  return http_request_->user_agent();
}

void CupEcdsaRequestImpl::set_user_agent(const CString& user_agent) {
  http_request_->set_user_agent(user_agent);
}

void CupEcdsaRequestImpl::set_proxy_auth_config(const ProxyAuthConfig& config) {
  http_request_->set_proxy_auth_config(config);
}

HRESULT CupEcdsaRequestImpl::BuildRequest() {
  // Generate a random nonce of 256 bits.
  char nonce[32] = {0};
  if (!RandBytes(&nonce, sizeof(nonce))) {
    metric_cup_ecdsa_other_errors++;
    return OMAHA_NET_E_CUP_NO_ENTROPY;
  }

  CStringA nonce_string;
  WebSafeBase64Escape(nonce, sizeof(nonce), &nonce_string, false);

  // Compute the SHA-256 hash of the request body; we need it to verify the
  // response, and we can optionally send it to the server as well.
  VERIFY1(SafeSHA256Hash(request_buffer_, request_buffer_length_,
                         &cup_->request_hash));

  // Generate the values of our query parameters, cup2key and (opt) cup2hreq.
  SafeCStringFormat(&cup_->cup2key, _T("%d:%S"),
                    static_cast<int>(public_key_.version()),
                    nonce_string);
  cup_->cup2hreq = BytesToHex(cup_->request_hash);

  // Compute the url of the CUP request -- append a query string, or append to
  // the existing query string if one already exists.
  ASSERT1(!url_.IsEmpty());
  const TCHAR* format_string = url_.Find(_T('?')) != -1 ?
      _T("%1&cup2key=%2&cup2hreq=%3") :
      _T("%1?cup2key=%2&cup2hreq=%3");
  cup_->request_url.FormatMessage(format_string,
                                  url_,
                                  cup_->cup2key,
                                  cup_->cup2hreq);
  NET_LOG(L4, (_T("[CUP-ECDSA][request:     %s]"), cup_->request_url));

  return S_OK;
}

HRESULT CupEcdsaRequestImpl::DoSend() {
  // Set the URL of the inner request to the full CUP url.
  http_request_->set_url(CString(cup_->request_url));

  NET_LOG(L5, (_T("[CUP-ECDSA request][%s]"),
               BufferToPrintableString(request_buffer_,
                                       request_buffer_length_)));
  HRESULT hr = http_request_->Send();
  if (FAILED(hr)) {
    metric_cup_ecdsa_http_failure++;
    return hr;
  }

  // Save the response body, and make sure we got an HTTP 200 or 206.
  http_request_->GetResponse().swap(cup_->response);
  int status_code(http_request_->GetHttpStatusCode());
  if (status_code != HTTP_STATUS_OK &&
      status_code != HTTP_STATUS_PARTIAL_CONTENT) {
    metric_cup_ecdsa_http_failure++;
    return HRESULTFromHttpStatusCode(status_code);
  }
  NET_LOG(L5, (_T("[CUP-ECDSA response][%s]"),
               VectorToPrintableString(cup_->response)));

  // Get the server signature out of the ETag string; it will contain the
  // ECDSA signature and the SHA-256 hash of the observed client request.
  // We check the custom HTTP header first, then check ETag if the former is not
  // present or empty.
  CString etag_header;
  http_request_->QueryHeadersString(WINHTTP_QUERY_CUSTOM,
                                    kHeaderXETag,
                                    &etag_header);
  if (etag_header.IsEmpty()) {
    http_request_->QueryHeadersString(WINHTTP_QUERY_ETAG, NULL, &etag_header);
  }

  cup_->etag = etag_header;

  return S_OK;
}

HRESULT CupEcdsaRequestImpl::AuthenticateResponse() {
  // Server should send, with the response, an ETag header containing the ECDSA
  // signature and the observed hash of the request.
  NET_LOG(L4, (_T("[CUP-ECDSA][etag:        %s]"), cup_->etag));

  if (cup_->etag.IsEmpty()) {
    CString response_as_string = Utf8BufferToWideChar(cup_->response);
    if (NULL == stristrW(response_as_string, L"<response") &&
        NULL != stristrW(response_as_string, L"<html")) {
      NET_LOG(L4, (_T("[CUP-ECDSA][Captive portal detected, aborting]")));
      metric_cup_ecdsa_captive_portal++;
      return OMAHA_NET_E_CAPTIVEPORTAL;
    } else {
      NET_LOG(L4, (_T("[CUP-ECDSA][ETag empty, rejecting]")));
      metric_cup_ecdsa_no_etag++;
      return OMAHA_NET_E_CUP_NO_SERVER_PROOF;
    }
  }

  // Compute the hash of the response body.  (Should be in UTF-8.)
  std::vector<uint8> response_hash;
  VERIFY1(SafeSHA256Hash(cup_->response, &response_hash));
  NET_LOG(L4, (_T("[CUP-ECDSA][resp hash][%s]"), BytesToHex(response_hash)));

  // Parse the ETag into its respective components.
  if (!ParseServerETag(cup_->etag,
                       &cup_->signature,
                       &cup_->observed_hash)) {
    NET_LOG(L4, (_T("[CUP-ECDSA][ETag invalid - rejecting]")));
    metric_cup_ecdsa_malformed_etag++;
    return OMAHA_NET_E_CUP_ECDSA_CORRUPT_ETAG;
  }

  // Compare the hash of our request-as-sent with the hash of the request that
  // the server received.  If they're not equal, we know that our request was
  // tampered while in flight to the server, and we can reject it quickly.
  // (Verifying that the server's reported observed hash wasn't tampered with
  // on the way back from the server is a more costly operation, and will be
  // performed next.)
  if (cup_->observed_hash.size() != SHA256_DIGEST_SIZE) {
    return OMAHA_NET_E_CUP_ECDSA_CORRUPT_ETAG;
  }
  if (cup_->request_hash.size() != SHA256_DIGEST_SIZE) {
    return E_UNEXPECTED;
  }
  if (!std::equal(cup_->observed_hash.begin(), cup_->observed_hash.end(),
                  cup_->request_hash.begin())) {
    NET_LOG(L4, (_T("[CUP-ECDSA][request hash mismatch - rejecting]")));
    metric_cup_ecdsa_request_hash_mismatch++;
    return OMAHA_NET_E_CUP_ECDSA_NOT_TRUSTED_REQUEST;
  }

  // The ECDSA signature signs the following buffer:
  //   hash( hash(request) | hash(response) | cup2key )
  // The "cup2key" param from the request contains keyid and nonce as a string,
  // encoded in UTF-8.

  std::vector<uint8> signed_message;

  signed_message.insert(signed_message.end(),
                        cup_->request_hash.begin(), cup_->request_hash.end());

  signed_message.insert(signed_message.end(),
                        response_hash.begin(), response_hash.end());

  std::vector<uint8> cup2key_as_utf8;
  WideToUtf8Vector(cup_->cup2key, &cup2key_as_utf8);
  signed_message.insert(signed_message.end(),
                        cup2key_as_utf8.begin(), cup2key_as_utf8.end());

  std::vector<uint8> digest;
  VERIFY1(SafeSHA256Hash(signed_message, &digest));

  NET_LOG(L4, (_T("[CUP-ECDSA][verified-message][%s]"), BytesToHex(digest)));

  // Verify the signature.
  if (!VerifyEcdsaSignature(public_key_, digest, cup_->signature)) {
    NET_LOG(L4, (_T("[CUP-ECDSA][signature is invalid - rejecting]")));
    metric_cup_ecdsa_signature_mismatch++;
    return OMAHA_NET_E_CUP_ECDSA_NOT_TRUSTED_SIGNATURE;
  }

  // If we've gotten here, the ECDSA signature for the assembled body is
  // valid.  That implies that the server response body is intact, as is
  // the hash that the server sent to us of the observed request.  To get here
  // at all implies that the observed request hash matches the sent request
  // hash, and that the nonces match.  Therefore, the transmission is intact.
  NET_LOG(L4, (_T("[CUP-ECDSA][signature is valid]")));
  metric_cup_ecdsa_trusted++;
  return S_OK;
}

// The ETag string is formatted as "S:H" where:
// * S is the ECDSA signature in ASN.1 DER-encoded form.
// * H is the SHA-256 hash of the observed request body, standard hex format.
// A Weak ETag is formatted as W/"S:H". This function treats it the same as a
// strong ETag.
bool CupEcdsaRequestImpl::ParseServerETag(const CString& etag_in,
                                          EcdsaSignature* sig_out,
                                          std::vector<uint8>* req_hash_out) {
  ASSERT1(sig_out);
  ASSERT1(req_hash_out);

  CString etag(etag_in);

  // Remove Weak ETag prefix if it exists.
  if (String_StartsWith(etag, kWeakETagPrefix, true)) {
    etag = etag.Mid(arraysize(kWeakETagPrefix) - 1);
  }

  // Remove double-quotes enclosing "S:H", if any.
  UnenclosePath(&etag);

  // Start by breaking them out of their delimiters.
  CString sig_str;
  CString req_hash_str;
  if (!ParseNameValuePair(etag, _T(':'), &sig_str, &req_hash_str)) {
    return false;
  }

  // The request hash should be exactly 256 bits, or 64 hex characters.
  // (The ECDSA signature varies in size; EcdsaSignature::DecodeFromBuffer()
  // will deal with validating that.)
  const size_t k256BitsInBytes = 256 / 4;  // 4 bits per nibble.
  if (req_hash_str.GetLength() != k256BitsInBytes) {
    return false;
  }

  // Convert both pieces from hex strings to byte vectors.  (Note that
  // SafeHexStringToVector() rejects empty strings.)
  std::vector<uint8> sig_bytes;
  std::vector<uint8> req_hash_bytes;
  if (!SafeHexStringToVector(sig_str, &sig_bytes) ||
      !SafeHexStringToVector(req_hash_str, &req_hash_bytes)) {
    return false;
  }

  // Parse the ECDSA signature.  (It will clean itself up on failure.)
  if (!sig_out->DecodeFromBuffer(sig_bytes)) {
    return false;
  }

  // If the ECDSA signature is valid, move the request hash to the output.
  req_hash_out->swap(req_hash_bytes);
  return true;
}

CupEcdsaRequestImpl::TransientCupState::TransientCupState() {}

}   // namespace internal

CupEcdsaRequest::CupEcdsaRequest(HttpRequestInterface* http_request) {
  ASSERT1(http_request);
  impl_.reset(new internal::CupEcdsaRequestImpl(http_request));
}

CupEcdsaRequest::~CupEcdsaRequest() {
}

HRESULT CupEcdsaRequest::Close() {
  return impl_->Close();
}

HRESULT CupEcdsaRequest::Send() {
  return impl_->Send();
}

HRESULT CupEcdsaRequest::Cancel() {
  return impl_->Cancel();
}

HRESULT CupEcdsaRequest::Pause() {
  return impl_->Pause();
}

HRESULT CupEcdsaRequest::Resume() {
  return impl_->Resume();
}

std::vector<uint8> CupEcdsaRequest::GetResponse() const {
  return impl_->GetResponse();
}

int CupEcdsaRequest::GetHttpStatusCode() const {
  return impl_->GetHttpStatusCode();
}

HRESULT CupEcdsaRequest::QueryHeadersString(uint32 info_level,
                                            const TCHAR* name,
                                            CString* value) const {
  return impl_->QueryHeadersString(info_level, name, value);
}

CString CupEcdsaRequest::GetResponseHeaders() const {
  return impl_->GetResponseHeaders();
}

CString CupEcdsaRequest::ToString() const {
  return impl_->ToString();
}

void CupEcdsaRequest::set_session_handle(HINTERNET session_handle) {
  return impl_->set_session_handle(session_handle);
}

void CupEcdsaRequest::set_url(const CString& url) {
  impl_->set_url(url);
}

void CupEcdsaRequest::set_request_buffer(const void* buffer,
                                         size_t buffer_length) {
  impl_->set_request_buffer(buffer, buffer_length);
}

void CupEcdsaRequest::set_proxy_configuration(const ProxyConfig& proxy_config) {
  impl_->set_proxy_configuration(proxy_config);
}

void CupEcdsaRequest::set_filename(const CString& filename) {
  impl_->set_filename(filename);
}

void CupEcdsaRequest::set_low_priority(bool low_priority) {
  impl_->set_low_priority(low_priority);
}

void CupEcdsaRequest::set_callback(NetworkRequestCallback* callback) {
  impl_->set_callback(callback);
}

void CupEcdsaRequest::set_additional_headers(
    const CString& additional_headers) {
  impl_->set_additional_headers(additional_headers);
}

CString CupEcdsaRequest::user_agent() const {
  return impl_->user_agent();
}

void CupEcdsaRequest::set_user_agent(const CString& user_agent) {
  impl_->set_user_agent(user_agent);
}

void CupEcdsaRequest::set_proxy_auth_config(const ProxyAuthConfig& config) {
  impl_->set_proxy_auth_config(config);
}

bool CupEcdsaRequest::download_metrics(DownloadMetrics* dm) const {
  UNREFERENCED_PARAMETER(dm);
  return false;
}

}   // namespace omaha
