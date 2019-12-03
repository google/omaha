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

#include "omaha/common/google_signaturevalidator.h"

#include <windows.h>
#include <atlstr.h>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/const_code_signing.h"
#include "omaha/base/signaturevalidator.h"

namespace omaha {

HRESULT VerifyGoogleAuthenticodeSignature(const CString& filename,
                                          bool allow_network_check) {
  HRESULT hr = VerifyAuthenticodeSignature(filename, allow_network_check);
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<CString> expected_hashes;
  for (size_t i = 0; i != arraysize(kPublicKeyHashes); ++i) {
    expected_hashes.push_back(kPublicKeyHashes[i]);
  }

  std::vector<CString> subject;
  subject.push_back(kSha256CertificateSubjectName);
  subject.push_back(kSha1CertificateSubjectName);
  subject.push_back(kLegacyCertificateSubjectName);

  const bool check_cert_is_valid_now = false;
  hr = VerifyCertificate(filename,
                         subject,
                         check_cert_is_valid_now,
                         expected_hashes.empty() ? NULL : &expected_hashes);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

}  // namespace omaha

