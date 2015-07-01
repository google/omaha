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

#ifndef OMAHA_COMMON_GOOGLE_SIGNATUREVALIDATOR_H_
#define OMAHA_COMMON_GOOGLE_SIGNATUREVALIDATOR_H_

#include <windows.h>
#include <atlstr.h>

namespace omaha {

// Returns true if the signee is Google by exactly matching the first CN name
// against a well-defined string, currently "Google Inc" or "Google Inc (TEST).
// The function enforces pinning to specific certificates and it only returns
// true if the hash of the public key of the certificate is found in a list
// of expected hashes. The list is currently hardcoded in the program image.
// The function does not verify that the certificate is currently valid and it
// will accept a signature with an expired certificate which was valid at the
// time of signing.
HRESULT VerifyGoogleAuthenticodeSignature(const CString& filename,
                                          bool allow_network_check);

}  // namespace omaha

#endif  // OMAHA_COMMON_GOOGLE_SIGNATUREVALIDATOR_H_

