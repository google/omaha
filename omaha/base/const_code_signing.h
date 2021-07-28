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
const TCHAR* const kLegacyCertificateSubjectName = _T("Google Inc");
const TCHAR* const kSha1CertificateSubjectName = _T("Google LLC");
const TCHAR* const kSha256CertificateSubjectName = _T("Google LLC");

// The Omaha certificate thumbprint. Used by unit tests.
const TCHAR* const kCertificateThumbprint =
    _T("a3958ae522f3c54b878b20d7b0f63711e08666b2");
const TCHAR* const kSha256CertificateThumbprint =
    _T("2673ea6cc23beffda49ac715b121544098a1284c");

// The SHA256 hash of the Omaha certificate RSA public key.
const TCHAR* const kCertificatePublicKeyHash =
    _T("6cb128676c6d0b49d3e8918bd835888694333da7540a0994261c0ec0b3516f9d");
const TCHAR* const kSha256CertificatePublicKeyHash =
    _T("3e9d92dfb3a046d49f53bab836f387177ac1ec075e8e3dd306b7c1764432f276");

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
    // SHA1
    // Fingerprint=06:C9:2B:EC:3B:BF:32:06:8C:B9:20:85:63:D0:04:16:94:48:EE:21
    _T("c7b4d0bf956f7ebbbc7369786f111ee6caa225af173be135e1de9e5a1d11951a"),

    // Omaha and Chrome certificate: sha1 (01/28/2014 to 01/29/2016).
    // thumbprint=fcac7e666cc54341ca213becf2eb463f2b62adb0
    // serial=2912C70C9A2B8A3EF6F6074662D68B8D
    // SHA1
    // Fingerprint=FC:AC:7E:66:6C:C5:43:41:CA:21:3B:EC:F2:EB:46:3F:2B:62:AD:B0
    _T("4365c47f17727f2da65892b1f34c0cf418b0138b519b6864dd17300f21aa3144"),

    // Omaha and Chrome certificate: sha1 (12/13/2015 to 12/14/2016).
    // thumbprint=264e38570f882e5a0272423757741233a661b553
    // serial=4c40dba5f988fae57a57d6457495f98b
    // SHA1
    // Fingerprint=26:4E:38:57:0F:88:2E:5A:02:72:42:37:57:74:12:33:A6:61:B5:53
    _T("309bae1b466c4235e1daea9fe0e373b3415807ac667202f704d030ef33b519d6"),

    // Omaha and Chrome certificate: sha256 (12/15/2015 to 12/16/2018).
    // thumbprint=5a9272ce76a9415a4a3a5002a2589a049312aa40
    // serial=2a9c21acaaa63a3c58a7b9322bee948d
    // SHA1
    // Fingerprint=5A:92:72:CE:76:A9:41:5A:4A:3A:50:02:A2:58:9A:04:93:12:AA:40
    _T("cd623b2bf2c06940bd480b6bcf4a5c9e1cbe94626fbfa127d001bf19ae5ba9fe"),

    // Omaha and Chrome certificate: sha1 (11/28/2016 to 11/21/2019).
    // thumbprint=1a6ac0549a4a44264deb6ff003391da2f285b19f
    // serial=14F8FDD167F92402B1570B5DC495C815
    // SHA1
    // Fingerprint=1A:6A:C0:54:9A:4A:44:26:4D:EB:6F:F0:03:39:1D:A2:F2:85:B1:9F
    _T("d49de35a2e9fdbed09e2b9a6c1243df414d6aac13690ab221b0017a5cbe1351f"),

    // Omaha certificate: sha1 (11/07/2019 to 11/16/2022).
    // thumbprint=a3958ae522f3c54b878b20d7b0f63711e08666b2
    // serial=06aea76bac46a9e8cfe6d29e45aaf033
    // SHA1
    // Fingerprint=A3:95:8A:E5:22:F3:C5:4B:87:8B:20:D7:B0:F6:37:11:E0:86:66:B2
    kCertificatePublicKeyHash,

    // Omaha and Chrome certificate: sha256 (11/06/2018 to 11/17/2021).
    // thumbprint=cb7e84887f3c6015fe7edfb4f8f36df7dc10590e
    // serial=0c15be4a15bb0903c901b1d6c265302f
    // SHA1
    // Fingerprint=CB:7E:84:88:7F:3C:60:15:FE:7E:DF:B4:F8:F3:6D:F7:DC:10:59:0E
    _T("03e27c19d222043a8f0c64181c23c9339cc84a7ec4ebff8a19adb7caefb0c709"),

    // Google LLC sha256 certificate valid from 07-01-2021 to 07-10-2024.
    // Issued by DigiCert Trusted G4 Code Signing RSA4096 SHA384 2021 CA1.
    // thumbprint=2673ea6cc23beffda49ac715b121544098a1284c.
    // serial=0e4418e2dede36dd2974c3443afb5ce5.
    kSha256CertificatePublicKeyHash,
};

}  // namespace omaha

#endif  // OMAHA_BASE_CONST_CODE_SIGNING_H_
