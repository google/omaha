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


#ifndef OMAHA_COMMON_EXTRACTOR_H_
#define OMAHA_COMMON_EXTRACTOR_H_

#include <windows.h>

namespace omaha {

class TagExtractor {
 public:
  TagExtractor();
  virtual ~TagExtractor();

    /**
    * @return true if we successfully opened the file.
    */
    bool OpenFile(const TCHAR* filename);

    /**
    * @return true if we currently have a handle to an open file.
    */
    bool IsFileOpen() const;

    void CloseFile();

    /**
    * Returns the tag in the current file.
    *
    * We're exploiting the empirical observation that Windows checks the
    * signature on a PEF but doesn't care if the signature container includes
    * extra bytes after the signature.
    *
    * Logic:
    *
    *   - Sanity-check that we're a PEF image.
    *   - Find the signature, which should be stored in the PE "Certificates
    *     Directory" (dumpbin.exe /headers "Firefox Setup 1.0.7.exe") in a
    *     WIN_CERTIFICATE structure.
    *   - Crudely parse the ASN.1 signature to determine its end.
    *   - Read the signature starting from the first byte past the ASN.1.
    *
    * @param tag_buffer: a buffer that will be filled with the extracted tag as
    *   a null-terminated string, or NULL if the caller doesn't want the tag.
    *
    * @param tag_buffer_len: a pointer to an int that represents the length in
    *   bytes of the buffer pointed to by tag_buffer. If tag_buffer is NULL and
    *   there is a tag to extract, then we fill this int with the size of the
    *   smallest buffer needed to contain the tag (plus the null terminator).
    *
    * @return true if we found a tag and either successfully copied all of it
    *   into tag_buffer, or tag_buffer was NULL and we successfully returned
    *   the required buffer size in tag_buffer_len.
    */
    bool ExtractTag(char* tag_buffer, int* tag_buffer_len);
    bool ExtractTag(const char* binary_file,
                    size_t binary_file_length,
                    char* tag_buffer,
                    int* tag_buffer_len);

    int cert_length() const { return cert_length_; }
    const void* cert_dir_base() const { return cert_dir_base_; }

 private:
  HANDLE file_handle_;
  HANDLE file_mapping_;
  LPVOID file_base_;
  size_t file_length_;
  int cert_length_;
  const void* cert_dir_base_;

  bool ReadTag(const char* tag_pointer,
               char* tag_buffer,
               int* tag_buffer_len) const;

  const void* GetCertificateDirectoryPointer(const void* base) const;

  const void* GetASN1SignaturePointer(const void* base) const;

  int GetASN1SignatureLength(const void* base) const;

  bool InternalExtractTag(const char* file_buffer,
                          char* tag_buffer,
                          int* tag_buffer_len);

  bool InternalReadCertificate(const char* file_buffer);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_EXTRACTOR_H_
