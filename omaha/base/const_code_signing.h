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

// The parameters of the current code signing Authenticode certificates. For
// security reasons, the checks involving Authenticode not only check the
// subject of the certificate but they check that specific certificates have
// been used to sign.
// The SHA256 hashes of the certificate RSA public keys are defined below.

#ifndef OMAHA_BASE_CONST_CODE_SIGNING_H_
#define OMAHA_BASE_CONST_CODE_SIGNING_H_

#include <windows.h>
#include <tchar.h>

namespace omaha {

// The company and organization names expected in the code
// signing certificates which are trusted.
const TCHAR* const kCertificateSubjectName = _T("Google Inc");

// The Omaha certificate thumbprint. Used by unit tests.
const TCHAR* const kCertificateThumbprint =
    _T("fcac7e666cc54341ca213becf2eb463f2b62adb0");

// The SHA256 hash of the Omaha certificate RSA public key.
const TCHAR* const kCertificatePublicKeyHash =
    _T("4365c47f17727f2da65892b1f34c0cf418b0138b519b6864dd17300f21aa3144");

// The hash of public keys that we pin the code signing certificates to.
// For quick identification, the date and thumbprint of the certificates are
// provide below. The hash is the SHA256 hash of the raw certificate RSA public
// key bytes in DER format.
const TCHAR* const kPublicKeyHashes[] = {
// Omaha certificate: (11/9/2011 to 11/9/2014).
// thumbprint=8aed552a1387870a53f5f8aee17a3761232a4609
_T("64637c145ee0b7888af408ec24f714242fc4da6b8ad7a04803254bf93f7d295f"),

// Chrome certificate: (11/13/2011 to 11/13/2014) revoked on 1/28/2014.
// thumbprint=06c92bec3bbf32068cb9208563d004169448ee21
// serial=09E28B26DB593EC4E73286B66499C370
// SHA1 Fingerprint=06:C9:2B:EC:3B:BF:32:06:8C:B9:20:85:63:D0:04:16:94:48:EE:21
_T("c7b4d0bf956f7ebbbc7369786f111ee6caa225af173be135e1de9e5a1d11951a"),

// Omaha and Chrome certificate: sha1 (01/28/2014 to 01/29/2016).
// thumbprint=fcac7e666cc54341ca213becf2eb463f2b62adb0
// serial=2912C70C9A2B8A3EF6F6074662D68B8D
// SHA1 Fingerprint=FC:AC:7E:66:6C:C5:43:41:CA:21:3B:EC:F2:EB:46:3F:2B:62:AD:B0
kCertificatePublicKeyHash,
};

}  // namespace omaha

#endif  // OMAHA_BASE_CONST_CODE_SIGNING_H_
