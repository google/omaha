// Copyright 2011 Google Inc.
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

#include "omaha/base/omaha_version.h"
#include "omaha/base/utils.h"
#include "omaha/core/core_launcher.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  omaha::EnableSecureDllLoading();

  scoped_co_init init_com_apt(COINIT_MULTITHREADED);
  HRESULT hr = init_com_apt.hresult();
  if (FAILED(hr)) {
    return hr;
  }

  bool is_system = false;

  omaha::InitializeVersionFromModule(NULL);
  hr = omaha::IsSystemProcess(&is_system);
  if (FAILED(hr)) {
    return hr;
  }

  return omaha::StartCoreIfNeeded(is_system);
}
