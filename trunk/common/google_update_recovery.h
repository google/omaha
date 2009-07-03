// Copyright 2007-2009 Google Inc.
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
// Defines the interface to the Google Update recovery mechanism to be included
// in Google apps.

#ifndef OMAHA_COMMON_GOOGLE_UPDATE_RECOVERY_H__
#define OMAHA_COMMON_GOOGLE_UPDATE_RECOVERY_H__

#include <tchar.h>
#include <windows.h>

#ifndef UNICODE
#error The distributed library only supports UNICODE.
#endif

extern "C" {
typedef HRESULT (*DownloadCallback)(const TCHAR* url,
                                    const TCHAR* file_path,
                                    void* context);

// Determines whether there is a Code Red event for the current installation
// and repairs it if necessary.
// app_language should follow the external Internet standard
// Best Common Practice (BCP) 47: http://www.rfc-editor.org/rfc/bcp/bcp47.txt
// context can be NULL if download_callback does not use it.
HRESULT FixGoogleUpdate(const TCHAR* app_guid,
                        const TCHAR* app_version,
                        const TCHAR* app_language,
                        bool is_machine_app,
                        DownloadCallback download_callback,
                        void* context);
}  // extern "C"

#endif  // OMAHA_COMMON_GOOGLE_UPDATE_RECOVERY_H__
