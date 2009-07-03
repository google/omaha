// Copyright 2003-2009 Google Inc.
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
// Encryption functions

#ifndef OMAHA_COMMON_ENCRYPT_H__
#define OMAHA_COMMON_ENCRYPT_H__

#include <windows.h>
#include <wincrypt.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

namespace encrypt {

// Encrypts a data buffer by using the user's credentials as well as
// the provided key. The key may be NULL, if no entropy is needed.
HRESULT EncryptData(const void* key, size_t key_len,
                    const void* data, size_t data_len,
                    std::vector<uint8>* data_out);

// Decrypts the provided data buffer. The key may be NULL, if no entropy
// is needed.
HRESULT DecryptData(const void* key, size_t key_len,
                    const void* data, size_t data_len,
                    std::vector<uint8>* data_out);

}  // namespace encrypt

}  // namespace omaha

#endif  // OMAHA_COMMON_ENCRYPT_H__
