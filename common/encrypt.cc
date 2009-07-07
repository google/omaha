// Copyright 2006-2009 Google Inc.
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


#include "omaha/common/encrypt.h"

#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"

namespace omaha {

namespace encrypt {

// TODO(omaha): consider loading crypt32.dll dynamically, as these functions
// are used infrequently.

HRESULT EncryptData(const void* key, size_t key_len,
                    const void* data, size_t data_len,
                    std::vector<uint8>* data_out) {
  // key may be null.
  ASSERT1(data);
  ASSERT1(data_out);
  DATA_BLOB blob_out = {0, NULL};
  DATA_BLOB blob_in = { data_len, static_cast<BYTE*>(const_cast<void*>(data)) };
  DATA_BLOB entropy = { 0, NULL };
  if (key != NULL && key_len != 0) {
    entropy.cbData = key_len;
    entropy.pbData = static_cast<BYTE*>(const_cast<void*>(key));
  }

  // The description parameter is required on W2K.
  if (!::CryptProtectData(&blob_in, _T("gupdate"), &entropy, NULL, NULL,
                          CRYPTPROTECT_UI_FORBIDDEN, &blob_out)) {
    return HRESULTFromLastError();
  }

  data_out->clear();
  const uint8* first = reinterpret_cast<const uint8*>(blob_out.pbData);
  const uint8* last = first + blob_out.cbData;
  data_out->insert(data_out->begin(), first, last);
  ::LocalFree(blob_out.pbData);

  ASSERT1(data_out->size() == blob_out.cbData);
  return S_OK;
}

HRESULT DecryptData(const void* key, size_t key_len,
                    const void* data, size_t data_len,
                    std::vector<uint8>* data_out) {
  // key may be null.
  ASSERT1(data);
  ASSERT1(data_out);

  DATA_BLOB blob_out = {0, NULL};
  DATA_BLOB blob_in = { data_len, static_cast<BYTE*>(const_cast<void*>(data)) };
  DATA_BLOB entropy = { 0, NULL };

  if (key != NULL && key_len != 0) {
    entropy.cbData = key_len;
    entropy.pbData = static_cast<BYTE*>(const_cast<void*>(key));
  }

  if (!::CryptUnprotectData(&blob_in, NULL, &entropy, NULL, NULL,
                            CRYPTPROTECT_UI_FORBIDDEN, &blob_out)) {
    return (::GetLastError() != ERROR_SUCCESS) ?
        HRESULTFromLastError() : HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
  }

  data_out->clear();
  const uint8* first = reinterpret_cast<const uint8*>(blob_out.pbData);
  const uint8* last = first + blob_out.cbData;
  data_out->insert(data_out->begin(), first, last);
  ::LocalFree(blob_out.pbData);

  ASSERT1(data_out->size() == blob_out.cbData);
  return S_OK;
}

}  // namespace encrypt

}  // namespace omaha

