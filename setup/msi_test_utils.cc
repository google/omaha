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

// IsMsiHelperInstalled requires MSI version 3.0 for MsiEnumProductsEx.
// Omaha does not require MSI 3.0.
#ifdef _WIN32_MSI
#if (_WIN32_MSI < 300)
#undef _WIN32_MSI
#define _WIN32_MSI 300
#endif
#else
#define _WIN32_MSI 300
#endif

#include <windows.h>
#include <msi.h>
#include "omaha/common/constants.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

bool IsMsiHelperInstalled() {
  int res = ::MsiEnumProductsEx(kHelperInstallerProductGuid,
                                NULL,
                                MSIINSTALLCONTEXT_MACHINE,
                                0,
                                NULL,
                                NULL,
                                NULL,
                                NULL);

  bool is_msi_installed = (ERROR_SUCCESS == res);
  EXPECT_TRUE(is_msi_installed || ERROR_NO_MORE_ITEMS == res);

  return is_msi_installed;
}

}  // namespace omaha
