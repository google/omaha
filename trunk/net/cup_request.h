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
// CupRequest provides CUP capabilities for a generic HttpRequestInterface.

#ifndef OMAHA_NET_CUP_REQUEST_H__
#define OMAHA_NET_CUP_REQUEST_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/net/http_request.h"
#include "base/scoped_ptr.h"

namespace omaha {

// The cup credentials are persisted across sessions. The sk is encrypted
// while on the disk so only a user with the same login credentials as
// the encryptor can decrypt it. The credentials are protected
// using the system default security, so users can't modify each other's
// credentials. In case of elevated administrators, the credentials are
// protected from the non-elevated administrators, so the latter can't
// read the keys and attack the elevated administrator.
//
// Cup credentials can be negotiated using either production keys or
// test keys. There is a registry value override to specify that test keys
// be used. For the change to be effective, the old credentials must be cleared.
struct CupCredentials {
  std::vector<uint8> sk;             // shared key (sk)
  CStringA c;                        // client cookie (c)
};

namespace detail {

// The implementation uses the pimpl idiom or bridge design pattern to
// encapsulate the cup protocol implementation details from the calling code.
class CupRequestImpl;

}   // namespace detail

class CupRequest : public HttpRequestInterface {
 public:
  // Decorates an HttpRequestInterface to provide CUP functionality.
  // It takes ownership of the object provided as parameter.
  explicit CupRequest(HttpRequestInterface* http_request);

  virtual ~CupRequest();

  virtual HRESULT Close();

  virtual HRESULT Send();

  virtual HRESULT Cancel();

  virtual std::vector<uint8> GetResponse() const;

  virtual HRESULT QueryHeadersString(uint32 info_level,
                                     const TCHAR* name,
                                     CString* value) const;
  virtual CString GetResponseHeaders() const;

  virtual int GetHttpStatusCode() const;

  virtual CString ToString() const;

  virtual void set_session_handle(HINTERNET session_handle);

  virtual void set_url(const CString& url);

  virtual void set_request_buffer(const void* buffer, size_t buffer_length);

  virtual void set_network_configuration(const Config& network_config);

  // Sets the filename to receive the response instead of the memory buffer.
  virtual void set_filename(const CString& filename);

  virtual void set_low_priority(bool low_priority);

  virtual void set_callback(NetworkRequestCallback* callback);

  virtual void set_additional_headers(const CString& additional_headers);

  virtual CString user_agent() const;

  virtual void set_user_agent(const CString& user_agent);

  // Sets random bytes provided by the caller. This is useful for testing
  // purposes and it is not be called by production code.
  // Otherwise, when entropy is not provided, the implementation
  // fills in the internal entropy buffer with cryptographically random bytes.
  // Calling this method can result in compromising the security of the
  // protocol, depending on the quality of entropy provided.
  void SetEntropy(const void* data, size_t data_length);

 private:

  scoped_ptr<detail::CupRequestImpl> impl_;
  DISALLOW_EVIL_CONSTRUCTORS(CupRequest);
};

}   // namespace omaha

#endif  // OMAHA_NET_CUP_REQUEST_H__

