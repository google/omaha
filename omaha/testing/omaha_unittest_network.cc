// Copyright 2007-2010 Google Inc.
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

#include "omaha/base/vistautil.h"
#include "omaha/base/reg_key.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/net/network_config.h"
#include "omaha/testing/omaha_unittest.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

int InitializeNetwork() {
  if (IsBuildSystem()) {
    // Until testsource can be set outside the tests on the build system,
    // tests that use the network must be run as admin so the it can be set.
    ASSERT1(vista_util::IsUserAdmin());
    SetBuildSystemTestSource();
  }

  // Ensure that any system running unittests has testsource set.
  // Some unit tests generate pings, and these must be filtered.
  CString value;
  HRESULT hr =
      RegKey::GetValue(MACHINE_REG_UPDATE_DEV, kRegValueTestSource, &value);
  if (FAILED(hr) || value.IsEmpty()) {
    ADD_FAILURE() << _T("'") << kRegValueTestSource << _T("'")
                  << _T(" is not present in ")
                  << _T("'") << MACHINE_REG_UPDATE_DEV << _T("'")
                  << _T(" or it is empty. Since you are running Omaha unit ")
                  << _T("tests, it should probably be set to 'dev' or 'qa'.");
    return -1;
  }

  // Many unit tests require the network configuration be initialized.
  // Referencing the singleton instance creats it if not exist.
  // On Windows Vista only admins can write to HKLM therefore the
  // initialization of the NetworkConfig must correspond to the integrity
  // level the user is running as.
  NetworkConfigManager::set_is_machine(vista_util::IsUserAdmin());
  NetworkConfigManager::Instance();

  return 0;
}

int DeinitializeNetwork() {
  NetworkConfigManager::DeleteInstance();
  return 0;
}

}  // namespace omaha
