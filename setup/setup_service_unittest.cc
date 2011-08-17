// Copyright 2008-2009 Google Inc.
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

#include "omaha/base/app_util.h"
#include "omaha/base/path.h"
#include "omaha/base/vistautil.h"
#include "omaha/setup/setup_service.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class SetupServiceTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    // The ServiceModule has it's own ATL module. ATL does not like having
    // multiple ATL modules. This TestCase saves and restore the original ATL
    // module to get around ATL's limitation. This is a hack.
    original_atl_module_ = _pAtlModule;
    _pAtlModule = NULL;
  }

  static void TearDownTestCase() {
    _pAtlModule = original_atl_module_;
  }

  static HRESULT DeleteUpdate3Service() {
    return SetupUpdate3Service::DeleteService();
  }

  static HRESULT DeleteMediumService() {
    return SetupUpdateMediumService::DeleteService();
  }

  static CAtlModule* original_atl_module_;
};

CAtlModule* SetupServiceTest::original_atl_module_ = NULL;

// TODO(omaha): Test SetupServiceTest

TEST_F(SetupServiceTest, InstallService_FileDoesNotExist) {
  if (!vista_util::IsUserAdmin()) {
    std::wcout << _T("\tTest did not run because the user is not an admin.")
               << std::endl;
    return;
  }

  DeleteUpdate3Service();
  DeleteMediumService();

  CString service_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                         _T("NoSuchFile.exe"));

  // The Windows service registration APIs do not rely on the file existing.
  EXPECT_SUCCEEDED(
      SetupUpdate3Service::InstallService(service_path));

  EXPECT_SUCCEEDED(
      SetupUpdateMediumService::InstallService(service_path));

  EXPECT_SUCCEEDED(DeleteUpdate3Service());
  EXPECT_SUCCEEDED(DeleteMediumService());
}

}  // namespace omaha
