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

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "omaha/common/file.h"
#include "omaha/common/module_utils.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/xml_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

const TCHAR kTestXMLFile[] = _T("manifest.xml");
const TCHAR kTempXMLFile[] = _T("foobar.xml");

const XMLFQName fqLowNoURI(NULL, _T("Bar"));
const XMLFQName fqLowNoURI2(NULL, _T("Bar"));
const XMLFQName fqHighNoURI(NULL, _T("Foo"));
const XMLFQName fqLowURI(_T("Zebra"), _T("Bar"));
const XMLFQName fqLowURI2(_T("Zebra"), _T("Bar"));
const XMLFQName fqHighURI(_T("Zebra"), _T("Foo"));
const XMLFQName fqDifferentURI(_T("Xray"), _T("Bar"));

TEST(XmlUtilsTest, XMLFQName) {
  ASSERT_TRUE(fqLowNoURI == fqLowNoURI2);
  ASSERT_TRUE(fqHighNoURI == fqHighNoURI);
  ASSERT_TRUE(fqLowNoURI != fqHighNoURI);
  ASSERT_TRUE(fqLowNoURI != fqLowURI);
  ASSERT_TRUE(fqLowNoURI < fqHighNoURI);
  ASSERT_TRUE(fqLowNoURI <= fqHighNoURI);
  ASSERT_TRUE(fqHighNoURI > fqLowNoURI);
  ASSERT_TRUE(fqHighNoURI >= fqLowNoURI);
  ASSERT_TRUE(fqLowURI == fqLowURI2);
  ASSERT_TRUE(fqHighURI == fqHighURI);
  ASSERT_TRUE(fqLowURI != fqHighURI);
  ASSERT_TRUE(fqLowURI < fqHighURI);
  ASSERT_TRUE(fqLowURI <= fqHighURI);
  ASSERT_TRUE(fqHighURI > fqLowURI);
  ASSERT_TRUE(fqHighURI >= fqLowURI);
  ASSERT_TRUE(fqLowURI != fqDifferentURI);
}

TEST(XmlUtilsTest, LoadSave) {
  scoped_co_init co_init;

  // Get some directory and file names to start with.
  TCHAR directory[MAX_PATH] = {0};
  ASSERT_TRUE(GetModuleDirectory(NULL, directory));
  CString test_file;
  test_file.AppendFormat(_T("%s\\%s"), directory, kTestXMLFile);

  TCHAR temp_path[MAX_PATH] = {0};
  ASSERT_TRUE(::GetTempPath(MAX_PATH, temp_path));
  CString temp_file;
  temp_file.AppendFormat(_T("%s%s"), temp_path, kTempXMLFile);

  // Test loading and storing to a file.
  CComPtr<IXMLDOMDocument> xmldoc;
  ASSERT_SUCCEEDED(LoadXMLFromFile(test_file, true, &xmldoc));
  ASSERT_TRUE(xmldoc);
  ASSERT_SUCCEEDED(SaveXMLToFile(xmldoc, temp_file));

  // Test loading and storing to memory.
  // The input must be Unicode but our test file is UTF-8 - so read
  // it and convert it to Unicode.
  std::vector<byte> buffer_utf8;
  ASSERT_SUCCEEDED(ReadEntireFile(temp_file, 0, &buffer_utf8));
  int len(::MultiByteToWideChar(CP_UTF8,
                                0, /*flags*/
                                reinterpret_cast<const char*>(&buffer_utf8[0]),
                                buffer_utf8.size(),
                                NULL,
                                0));
  std::vector<wchar_t> buffer_unicode(len+1);
  int len2(::MultiByteToWideChar(CP_UTF8,
                                 0, /*flags*/
                                 reinterpret_cast<const char*>(&buffer_utf8[0]),
                                 buffer_utf8.size(),
                                 &buffer_unicode[0],
                                 len));
  ASSERT_EQ(len, len2);
  buffer_unicode[len] = 0;  // null terminate the unicode string.

  // Now round-trip the load from memory and save to memory.
  CComPtr<IXMLDOMDocument> xmldoc2;
  ASSERT_SUCCEEDED(LoadXMLFromMemory(&buffer_unicode.front(), true, &xmldoc2));
  ASSERT_TRUE(xmldoc2);
  CString xmlmemory;
  ASSERT_SUCCEEDED(SaveXMLToMemory(xmldoc2, &xmlmemory));

  // Now compare that the result of the round-trip is the same as the input.
  CString input(&buffer_unicode.front());
  CString output(xmlmemory);
  // Except must first remove the " encoding="UTF-8"" attribute from the
  // input string.
  ReplaceCString(input, L" encoding=\"UTF-8\"", L"");
  ASSERT_STREQ(input, output);

  // Clean up.
  ASSERT_SUCCEEDED(File::Remove(temp_file));
}

}  // namespace omaha

