// Copyright 2019 Google LLC.
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

#ifndef OMAHA_GOOPDATE_DM_STORAGE_H__
#define OMAHA_GOOPDATE_DM_STORAGE_H__

#include <windows.h>
#include <atlstr.h>

#include "base/basictypes.h"
#include "omaha/base/constants.h"

namespace omaha {

// A handler for storage related to cloud-based device management of Omaha. This
// class provides access to an enrollment token, a device management token, and
// a device identifier.
class DmStorage {
 public:
  // The possible sources of an enrollment token, sorted by decreasing
  // precedence.
  enum EnrollmentTokenSource {
    kETokenSourceNone,
    kETokenSourceRuntime,
    kETokenSourceInstall,
    kETokenSourceCompanyPolicy,
#if defined(HAS_LEGACY_DM_CLIENT)
    kETokenSourceLegacyPolicy,
    kETokenSourceOldLegacyPolicy,
#endif  // defined(HAS_LEGACY_DM_CLIENT)
  };

  // Constructs an instance with a runtime-provided enrollment token (e.g., one
  // obtained via the etoken extra arg).
  explicit DmStorage(const CString& enrollment_token);

  // Returns the current enrollment token, reading from sources as-needed to
  // find one. Returns an empty string if no enrollment token is found.
  CString GetEnrollmentToken();

  // Returns the origin of the current enrollment token, or kETokenSourceNone if
  // none has been found.
  EnrollmentTokenSource enrollment_token_source() const {
    return enrollment_token_source_;
  }

  // Writes the instance's enrollment token into Omaha's ClientState key so that
  // it is available for subsequent runs.
  HRESULT StoreEnrollmentTokenForInstall();

  // Returns the device management token, reading from sources as-needed to find
  // one. Returns an empty string if no device management token is found.
  CStringA GetDmToken();

  // Writes |dm_token| into the registry.
  HRESULT StoreDmToken(const CStringA& dm_token);

  // Returns the device identifier, or an empty string in case of error.
  CString GetDeviceId();

 private:
  // The possible sources of a device management token, sorted by decreasing
  // precedence.
  enum DmTokenSource {
    kDmTokenSourceNone,
    kDmTokenSourceCompany,
#if defined(HAS_LEGACY_DM_CLIENT)
    kDmTokenSourceLegacy,
#endif  // defined(HAS_LEGACY_DM_CLIENT)
  };

  void LoadEnrollmentTokenFromStorage();
  void LoadDmTokenFromStorage();
  void LoadDeviceIdFromStorage();

  // The active enrollment token.
  CString enrollment_token_;

  // The active device management token.
  CStringA dm_token_;

  // The device identifier.
  CString device_id_;

  // The origin of the current enrollment token.
  EnrollmentTokenSource enrollment_token_source_;

  // The origin of the current device management token.
  DmTokenSource dm_token_source_;

  DISALLOW_COPY_AND_ASSIGN(DmStorage);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DM_STORAGE_H__
