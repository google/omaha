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

#include "base/scoped_ptr.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/common/command_line.h"
#include "omaha/common/exception_handler.h"
#include "omaha/testing/unit_test.h"

using google_breakpad::CustomInfoEntry;

namespace omaha {

class ExceptionHandlerTest : public testing::Test {
 protected:
  // Initialize the crash reporting for the machine case. The user case is
  // simpler and specific tests can reinitialize for the user case if needed.
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  HRESULT InstallHandler(bool is_machine,
                         const CustomInfoMap& custom_info_map) {
    if (handler_.get()) {
      handler_.reset();
    }

    return OmahaExceptionHandler::Create(is_machine,
                                         custom_info_map,
                                         address(handler_));
  }

  void UninstallHandler() {
    handler_.reset();
  }

  bool IsMachine() const {
    return handler_->is_machine();
  }

  CString GetCustomEntryValue(const CString& entry_name) const {
    for (std::vector<google_breakpad::CustomInfoEntry>::size_type i = 0;
         i != handler_->custom_entries_.size();
         ++i) {
      if (handler_->custom_entries_[i].name == entry_name) {
        return handler_->custom_entries_[i].value;
      }
    }

    return CString();
  }

  scoped_ptr<OmahaExceptionHandler> handler_;
};

// Installs and uninstalls the crash handler in the user case.
TEST_F(ExceptionHandlerTest, InstallCrashHandler_User) {
  EXPECT_HRESULT_SUCCEEDED(InstallHandler(false, CustomInfoMap()));
  EXPECT_FALSE(IsMachine());
}

// Installs and uninstalls the crash handler in the machine case.
TEST_F(ExceptionHandlerTest, InstallCrashHandler_Machine) {
  EXPECT_HRESULT_SUCCEEDED(InstallHandler(true, CustomInfoMap()));
  EXPECT_TRUE(IsMachine());
}

// Do a multiple install of the crash handler.
TEST_F(ExceptionHandlerTest, InstallCrashHandler_UserThenMachine) {
  EXPECT_HRESULT_SUCCEEDED(InstallHandler(false, CustomInfoMap()));
  EXPECT_FALSE(IsMachine());
  UninstallHandler();
  EXPECT_HRESULT_SUCCEEDED(InstallHandler(true, CustomInfoMap()));
  EXPECT_TRUE(IsMachine());
}

TEST_F(ExceptionHandlerTest, InstallCrashHandler_CustomInfoMap) {
  CustomInfoMap custom_info_map;
  CString command_line_mode;
  command_line_mode.Format(_T("%d"), COMMANDLINE_MODE_INSTALL);
  custom_info_map[kCrashCustomInfoCommandLineMode] = command_line_mode;
  CString long_value(_T('@'), MAX_PATH);
  custom_info_map[_T("FooBar")] =
      long_value.Left(CustomInfoEntry::kValueMaxLength - 1);

  EXPECT_HRESULT_SUCCEEDED(InstallHandler(false, custom_info_map));

  EXPECT_STREQ(_T("9"), GetCustomEntryValue(kCrashCustomInfoCommandLineMode));
  EXPECT_STREQ(
      _T("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"),
      GetCustomEntryValue(_T("FooBar")));
}

}  // namespace omaha

