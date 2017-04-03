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
//
// A Windows Installer custom action that extract tagged info from the MSI
// package file.

#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <string>

#include "omaha/enterprise/installer/custom_actions/msi_custom_action.h"
#include "omaha/enterprise/installer/custom_actions/msi_tag_extractor.h"

namespace {

const wchar_t kPropertyOriginalDatabase[] = L"OriginalDatabase";

// Extracts the tagged value from the source MSI package. If succeeded,
// the value is assigned to the given MSI property.
void ExtractAndSetProperty(
    MSIHANDLE msi_handle,
    const custom_action::MsiTagExtractor& tag_extractor,
    const char* tag_key_name,
    const wchar_t* property_name,
    size_t property_size_limit) {
  std::string tag_value;
  if (!tag_extractor.GetValue(tag_key_name, &tag_value) ||
      tag_value.length() > property_size_limit) {
    return;
  }

  const std::wstring property_value(tag_value.begin(), tag_value.end());
  ::MsiSetProperty(msi_handle, property_name, property_value.c_str());
}

}  // namespace

// A DLL custom action entrypoint that extracts the tagged information from the
// source MSI package. Usually, the extracted information will be set as MSI
// properties. Installer will pick up the values and pass them via command line
// to Omaha setup process.
extern "C" UINT __stdcall ExtractTagInfoFromInstaller(MSIHANDLE msi_handle) {
  std::wstring msi_path;
  if (!custom_action::GetProperty(msi_handle,
                                  kPropertyOriginalDatabase,
                                  &msi_path)) {
    return ERROR_SUCCESS;
  }

  custom_action::MsiTagExtractor tag_extractor;
  if (!tag_extractor.ReadTagFromFile(msi_path.c_str())) {
    return ERROR_SUCCESS;
  }

  const struct {
    const char* tag_name;
    const wchar_t* property_name;
    size_t property_size_limit;
  } kSupportedTags[] = {
    { "ap", L"AP", 32 },
    { "appguid", L"APPGUID", 38 },  // 128/4 nybbles + four dashes + two braces.
    { "appname", L"APPNAME", 512 },
    { "brand", L"BRAND", 4 },  // A four-character brand code.
    { "browser", L"BROWSER", 1 },  // A single integer digit BrowserType.
    { "iid", L"IID", 38 },  // 128/4 nybbles + four dashes + two braces.
    { "installdataindex", L"INSTALLDATAINDEX", 50 },
    { "lang", L"LANG", 10 },
    { "needsadmin", L"NEEDSADMIN", 7 },  // false/true/prefers.
    { "usagestats", L"USAGESTATS", 1 },  // A single integer digit tristate.
  };

  for (size_t i = 0; i < arraysize(kSupportedTags); ++i) {
    ExtractAndSetProperty(msi_handle, tag_extractor,
        kSupportedTags[i].tag_name,
        kSupportedTags[i].property_name,
        kSupportedTags[i].property_size_limit);
  }

  return ERROR_SUCCESS;
}

