// Copyright 2005-2009 Google Inc.
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
// Utility functions related to PE files (executables)

#include "omaha/common/pe_utils.h"

#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/scoped_any.h"

namespace omaha {

// Not really necessary as long as running on x86 architecture throughout, but
// what the hell
#if defined(BIG_ENDIAN)
uint32 GetUint32LE(void const * const p) {
  uchar const * const pu = reinterpret_cast<uchar const * const>(p);
  uint32 i = pu[0] | pu[1]<<8 | pu[2]<<16 | pu[3]<<24;
  return i;
}

void PutUint32LE(uint32 i, void * const p) {
  uchar * const pu = reinterpret_cast<uchar * const>(p);
  pu[0] = i & 0xff;
  pu[1] = (i >>  8) & 0xff;
  pu[2] = (i >> 16) & 0xff;
  pu[3] = (i >> 24) & 0xff;
}
#else  // LITTLE_ENDIAN
inline uint32 GetUint32LE(void const * const p) {
  uint32 const * const pu = reinterpret_cast<uint32 const * const>(p);
  return *pu;
}

inline void PutUint32LE(uint32 i, void * const p) {
  uint32 * const pu = reinterpret_cast<uint32 * const>(p);
  *pu = i;
}
#endif

// Magic PE constants
const uint32 kPEHeaderOffset         = 60;
const uint32 kPEHeaderChecksumOffset = 88;
const uint32 kPEHeaderSizeMin        = 160;
const char magic_EXE_header[] = "MZ\0\0";
const char magic_PE_header[]  = "PE\0\0";

HRESULT SetPEChecksum(const TCHAR *filename, uint32 checksum) {
  // Write the checksum field of the Windows NT-specific "optional" header.
  // Use Windows API calls rather than C library calls so that it will be
  // really a small routine when used in the stub executable.

  ASSERT(filename, (L""));

  scoped_hfile file(::CreateFile(filename,
                                 GENERIC_READ | GENERIC_WRITE,
                                 0,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL));
  if (!file)
    return HRESULTFromLastError();

  size_t size = ::GetFileSize(get(file), NULL);
  if (size == INVALID_FILE_SIZE)
    return HRESULTFromLastError();

  scoped_file_mapping mapping(::CreateFileMapping(get(file),
                                                  NULL,
                                                  PAGE_READWRITE,
                                                  0,
                                                  0,
                                                  NULL));
  if (!mapping)
    return HRESULTFromLastError();

  scoped_file_view file_data(::MapViewOfFile(get(mapping),
                                             FILE_MAP_WRITE,
                                             0,
                                             0,
                                             size));
  if (!file_data)
    return HRESULTFromLastError();

  uchar * image = reinterpret_cast<uchar *>(get(file_data));

  return SetPEChecksumToBuffer(image, size, checksum);
}

HRESULT GetPEChecksum(const TCHAR *filename, uint32 * checksum) {
  // Read the checksum field out of the Windows NT-specific "optional" header.
  // Use Windows API calls rather than C library calls so that it will be
  // really a small routine when used in the stub executable.

  ASSERT(filename, (L""));
  ASSERT(checksum, (L""));

  scoped_hfile file(::CreateFile(filename,
                                 GENERIC_READ,
                                 FILE_SHARE_READ,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_READONLY,
                                 NULL));
  if (!file)
    return HRESULTFromLastError();

  size_t size = ::GetFileSize(get(file), NULL);
  if (size == INVALID_FILE_SIZE)
    return HRESULTFromLastError();

  scoped_file_mapping mapping(::CreateFileMapping(get(file),
                              NULL,
                              PAGE_READONLY,
                              0,
                              0,
                              NULL));
  if (!mapping)
    return HRESULTFromLastError();

  scoped_file_view file_data(::MapViewOfFile(get(mapping),
                             FILE_MAP_READ,
                             0,
                             0,
                             size));
  if (!file_data)
    return HRESULTFromLastError();

  uchar * image = reinterpret_cast<uchar *>(get(file_data));

  return GetPEChecksumFromBuffer(image, size, checksum);
}

HRESULT SetPEChecksumToBuffer(uchar *buffer, size_t size, uint32 checksum) {
  // Sanity checks
  if (size < 64) {
    ASSERT(false, (L"File too short to be valid executable"));
    return E_FAIL;
  }

  uint32 x = GetUint32LE(magic_EXE_header);
  if (::memcmp(buffer, &x, 2)) {
    ASSERT(false, (L"Missing executable's magic number"));
    return E_FAIL;
  }

  uint32 peheader = GetUint32LE(buffer + kPEHeaderOffset);
  if (size < peheader + kPEHeaderSizeMin) {
    ASSERT(false, (L"Too small given PE header size"));
    return E_FAIL;
  }

  x = GetUint32LE(magic_PE_header);
  if (::memcmp(buffer + peheader, &x, 4)) {
    ASSERT(false, (L"Missing PE header magic number"));
    return E_FAIL;
  }

  // Finally, write the checksum
  PutUint32LE(checksum, &x);
  ::memcpy(buffer + peheader + kPEHeaderChecksumOffset, &x, 4);

  return S_OK;
}

HRESULT GetPEChecksumFromBuffer(const unsigned char *buffer,
                                size_t size,
                                uint32 *checksum) {
  // Sanity checks
  if (size < 64) {
    ASSERT(false, (L"File too short to be valid executable"));
    return E_FAIL;
  }

  uint32 x = GetUint32LE(magic_EXE_header);
  if (::memcmp(buffer, &x, 2)) {
    ASSERT(false, (L"Missing executable's magic number"));
    return E_FAIL;
  }

  uint32 peheader = GetUint32LE(buffer + kPEHeaderOffset);
  if (size < peheader + kPEHeaderSizeMin) {
    ASSERT(false, (L"Too small given PE header size"));
    return E_FAIL;
  }

  x = GetUint32LE(magic_PE_header);
  if (::memcmp(buffer + peheader, &x, 4)) {
    ASSERT(false, (L"Missing PE header magic number"));
    return E_FAIL;
  }

  // Finally, read the checksum

  *checksum = GetUint32LE(buffer + peheader + kPEHeaderChecksumOffset);

  return S_OK;
}

}  // namespace omaha

