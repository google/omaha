// Copyright 2008-2010 Google Inc.
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
#include "omaha/base/const_object_names.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/path.h"
#include "omaha/base/thread.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/scheduled_task_utils.h"
#include "omaha/core/core.h"
#include "omaha/core/core_launcher.h"
#include "omaha/goopdate/app_command_test_base.h"
#include "omaha/setup/setup_service.h"
#include "omaha/testing/unit_test.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

namespace {

// Runs the core on a different thread. Since the core captures the thread id
// in its constructor, the core instance must be created on this thread, not
// on the main thread.
class CoreRunner : public Runnable {
 public:
  explicit CoreRunner(bool is_machine) : is_machine_(is_machine) {}
  virtual ~CoreRunner() {}

 private:
  virtual void Run() {
    Core core;
    core.Main(is_machine_, false);         // Do not run the crash handler.
  }

  bool is_machine_;
  DISALLOW_COPY_AND_ASSIGN(CoreRunner);
};

}  // namespace

class CoreTest : public testing::Test {
 public:
  CoreTest() : is_machine_(false) {}

  virtual void SetUp() {
    // The Core has it's own ATL module. ATL does not like having multiple ATL
    // modules. This TestCase saves and restore the original ATL module to get
    // around ATL's limitation. This is a hack.
    original_atl_module_ = _pAtlModule;
    _pAtlModule = NULL;

    ASSERT_HRESULT_SUCCEEDED(IsSystemProcess(&is_machine_));

    ConfigManager::Instance()->SetLastCheckedTime(is_machine_, 10);

    NamedObjectAttributes attr;
    GetNamedObjectAttributes(kShutdownEvent, is_machine_, &attr);
    reset(shutdown_event_, ::CreateEvent(&attr.sa, true, false, attr.name));
    ASSERT_TRUE(shutdown_event_);
  }

  virtual void TearDown() {
     _pAtlModule = original_atl_module_;
  }

  HRESULT SignalShutdownEvent() {
    EXPECT_TRUE(valid(shutdown_event_));
    return ::SetEvent(get(shutdown_event_)) ? S_OK : HRESULTFromLastError();
  }

  HRESULT ResetShutdownEvent() {
    EXPECT_TRUE(valid(shutdown_event_));
    return ::ResetEvent(get(shutdown_event_)) ? S_OK : HRESULTFromLastError();
  }

 protected:
  bool is_machine_;
  scoped_event shutdown_event_;

  CAtlModule* original_atl_module_;
};

// Tests the core shutdown mechanism.
TEST_F(CoreTest, Shutdown) {
  // Signal existing core instances to shutdown, otherwise new instances
  // can't start.
  ASSERT_HRESULT_SUCCEEDED(SignalShutdownEvent());
  ::Sleep(0);
  ASSERT_HRESULT_SUCCEEDED(ResetShutdownEvent());

  // Start a thread to run the core, signal the core to exit, and wait a while
  // for the thread to exit. Terminate the thread if it is still running.
  Thread thread;
  CoreRunner core_runner(is_machine_);
  EXPECT_TRUE(thread.Start(&core_runner));

  // Give the core a little time to run before signaling it to exit.
  ::Sleep(100);
  EXPECT_HRESULT_SUCCEEDED(SignalShutdownEvent());
  EXPECT_TRUE(thread.WaitTillExit(2000));
  EXPECT_HRESULT_SUCCEEDED(ResetShutdownEvent());
}

TEST_F(CoreTest, HasOSUpgraded) {
  const TCHAR* const kAppGuid1 = _T("{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}");
  const TCHAR* const kCmdId1 = _T("CreateOSVersionsFileOnOSUpgrade");
  const TCHAR* const kCmdLineCreateOSVersionsFile =
      _T("cmd.exe /c \"echo %1 > %1 && exit 0\"");
  const TCHAR* const kCmdId2 = _T("CreateHardcodedFileOnOSUpgrade");
  const TCHAR* const kCmdLineCreateHardcodedFile =
      _T("cmd.exe /c \"echo HardcodedFile > HardcodedFile && exit 0\"");

  // Signal existing core instances to shutdown, otherwise new instances
  // can't start.
  ASSERT_HRESULT_SUCCEEDED(SignalShutdownEvent());
  ::Sleep(0);
  ASSERT_HRESULT_SUCCEEDED(ResetShutdownEvent());

  // Delete any existing LastOSVersion value.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(
      ConfigManager::Instance()->registry_update(is_machine_),
      kRegValueLastOSVersion));

  // Build a key with a lower major version to look like we upgraded.
  OSVERSIONINFOEX oldmajor = {};
  EXPECT_SUCCEEDED(SystemInfo::GetOSVersion(&oldmajor));
  --oldmajor.dwMajorVersion;
  EXPECT_SUCCEEDED(app_registry_utils::SetLastOSVersion(is_machine_,
                                                        &oldmajor));

  // Add in an App with OSUpgrade commands.
  omaha::AppCommandTestBase::CreateAppClientKey(kAppGuid1, is_machine_);
  omaha::AppCommandTestBase::CreateAutoRunOnOSUpgradeCommand(
      kAppGuid1, is_machine_, kCmdId1, kCmdLineCreateOSVersionsFile);
  omaha::AppCommandTestBase::CreateAutoRunOnOSUpgradeCommand(
      kAppGuid1, is_machine_, kCmdId2, kCmdLineCreateHardcodedFile);

  ConfigManager* cm = ConfigManager::Instance();
  const CString update_key = cm->registry_update(is_machine_);

  // Setting "LastCheckedTime" to current time prevents the core from
  // starting a "UA" process.
  const uint32 now_sec = Time64ToInt32(GetCurrent100NSTime());
  EXPECT_HRESULT_SUCCEEDED(cm->SetLastCheckedTime(is_machine_, now_sec));

  // Start a thread to run the core, signal the core to exit, and wait a while
  // for the thread to exit. Terminate the thread if it is still running.
  Thread thread;
  CoreRunner core_runner(is_machine_);
  EXPECT_TRUE(thread.Start(&core_runner));

  // Give the core a little time to run before signaling it to exit.
  ::Sleep(100);
  EXPECT_HRESULT_SUCCEEDED(SignalShutdownEvent());
  EXPECT_TRUE(thread.WaitTillExit(2000));

  CString os_upgrade_string;
  for (int i = 0; i < 2; ++i) {
    SafeCStringAppendFormat(&os_upgrade_string, _T("%u.%u.%u.%u.%u%s"),
                            oldmajor.dwMajorVersion + i,
                            oldmajor.dwMinorVersion,
                            oldmajor.dwBuildNumber,
                            oldmajor.wServicePackMajor,
                            oldmajor.wServicePackMinor,
                            i < 1 ? _T("-") : _T(""));
  }

  CString os_upgrade_file = ConcatenatePath(GetCurrentDir(), os_upgrade_string);

  // The app command runs concurrently, therefore, spin here for 10 seconds,
  // and wait for the file to appear.
  for (int i = 0; i != 100 && !File::Exists(os_upgrade_file); ++i) {
    Sleep(100);
  }

  EXPECT_TRUE(File::Exists(os_upgrade_file));

  CString hardcoded_file =
      ConcatenatePath(GetCurrentDir(), _T("HardcodedFile"));
  EXPECT_TRUE(File::Exists(hardcoded_file));

  // Cleanup.
  EXPECT_SUCCEEDED(File::Remove(os_upgrade_file));
  EXPECT_SUCCEEDED(File::Remove(hardcoded_file));
  EXPECT_HRESULT_SUCCEEDED(ResetShutdownEvent());
  omaha::AppCommandTestBase::DeleteAppClientKey(kAppGuid1, is_machine_);
  EXPECT_SUCCEEDED(RegKey::DeleteValue(
      ConfigManager::Instance()->registry_update(is_machine_),
      kRegValueLastOSVersion));
}

class CoreUtilsTest : public testing::Test {
 public:
  CoreUtilsTest() : is_machine_(vista_util::IsUserAdmin()) {}

  virtual void SetUp() {
    // The Core has it's own ATL module. ATL does not like having multiple ATL
    // modules. This TestCase saves and restore the original ATL module to get
    // around ATL's limitation. This is a hack.
    original_atl_module_ = _pAtlModule;
    _pAtlModule = NULL;

    // The Core must be created after the ATL module work around.
    core_.reset(new Core);
    core_->is_system_ = is_machine_;
  }

  virtual void TearDown() {
    _pAtlModule = original_atl_module_;
  }

  bool AreScheduledTasksHealthy() {
    return core_->AreScheduledTasksHealthy();
  }

  bool IsCheckingForUpdates() {
    return core_->IsCheckingForUpdates();
  }

  bool HasOSUpgraded() {
    return core_->HasOSUpgraded();
  }

  bool ShouldRunCore() {
    return omaha::ShouldRunCore(is_machine_);
  }

  bool ShouldRunCodeRed() {
    return core_->ShouldRunCodeRed();
  }

  static HRESULT DoInstallService(const TCHAR* service_cmd_line) {
    return SetupUpdate3Service::DoInstallService(service_cmd_line);
  }

  static HRESULT DeleteService() {
    return SetupUpdate3Service::DeleteService();
  }

  std::unique_ptr<Core> core_;
  bool is_machine_;

  CAtlModule* original_atl_module_;
};

TEST_F(CoreUtilsTest, AreScheduledTasksHealthy) {
  ConfigManager* cm = ConfigManager::Instance();

  const CString cs_key = cm->registry_client_state_goopdate(is_machine_);

  EXPECT_SUCCEEDED(scheduled_task_utils::UninstallGoopdateTasks(is_machine_));
  EXPECT_FALSE(AreScheduledTasksHealthy());

  CString task_path = ConcatenatePath(app_util::GetCurrentModuleDirectory(),
                                      _T("LongRunningSilent.exe"));
  EXPECT_SUCCEEDED(scheduled_task_utils::InstallGoopdateTasks(task_path,
                                                              is_machine_));
  const uint32 now_sec = Time64ToInt32(GetCurrent100NSTime());
  const int k12HourPeriodSec = 12 * 60 * 60;
  const int k36HourPeriodSec = 36 * 60 * 60;
  const DWORD back_12_hours_sec = now_sec - k12HourPeriodSec;
  const DWORD back_36_hours_sec = now_sec - k36HourPeriodSec;

  // Recent install.
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_12_hours_sec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_TRUE(AreScheduledTasksHealthy());

  // Recent update.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_12_hours_sec));
  EXPECT_TRUE(AreScheduledTasksHealthy());

  // Old install.
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_36_hours_sec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_FALSE(AreScheduledTasksHealthy());

  // Old update.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_36_hours_sec));
  EXPECT_FALSE(AreScheduledTasksHealthy());

  // Old install, recent update.
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_36_hours_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_12_hours_sec));
  EXPECT_TRUE(AreScheduledTasksHealthy());

  EXPECT_SUCCEEDED(scheduled_task_utils::UninstallGoopdateTasks(is_machine_));
}

TEST_F(CoreUtilsTest, IsCheckingForUpdates) {
  ConfigManager* cm = ConfigManager::Instance();

  const CString update_key = cm->registry_update(is_machine_);
  const CString cs_key = cm->registry_client_state_goopdate(is_machine_);

  const uint32 now_sec = Time64ToInt32(GetCurrent100NSTime());
  const int k12HourPeriodSec = 12 * 60 * 60;
  const int k36HourPeriodSec = 36 * 60 * 60;
  const int k15DaysSec = 14 * 24 * 60 * 60;
  const DWORD back_12_hours_sec = now_sec - k12HourPeriodSec;
  const DWORD back_36_hours_sec = now_sec - k36HourPeriodSec;
  const DWORD back_15days_sec   = now_sec - k15DaysSec;

  // No values are set => false.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_FALSE(IsCheckingForUpdates());

  // Recent install time only => too early to tell, assume true.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_12_hours_sec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_TRUE(IsCheckingForUpdates());

  // Old install time only => false.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_36_hours_sec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_FALSE(IsCheckingForUpdates());

  // Recent install and update times => too early to tell, assume true.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_12_hours_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_12_hours_sec));
  EXPECT_TRUE(IsCheckingForUpdates());

  // Old install but recent update times => too early to tell, assume true.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_36_hours_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_12_hours_sec));
  EXPECT_TRUE(IsCheckingForUpdates());

  // Old install and old update times => false.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_36_hours_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_36_hours_sec));
  EXPECT_FALSE(IsCheckingForUpdates());

  // No install time and recent update time => true.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_12_hours_sec));
  EXPECT_TRUE(IsCheckingForUpdates());

  // No install time, old update time => false.
  EXPECT_SUCCEEDED(RegKey::DeleteValue(update_key, kRegValueLastChecked));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_36_hours_sec));
  EXPECT_FALSE(IsCheckingForUpdates());


  // No install time, no update time, recent last checked => true.
  EXPECT_SUCCEEDED(cm->SetLastCheckedTime(is_machine_, back_12_hours_sec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_TRUE(IsCheckingForUpdates());

  // No install time, no update time, current last checked => true.
  EXPECT_SUCCEEDED(goopdate_utils::UpdateLastChecked(is_machine_));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_TRUE(IsCheckingForUpdates());

  // No install time, no update time, old last checked => false.
  EXPECT_SUCCEEDED(cm->SetLastCheckedTime(is_machine_, back_15days_sec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueInstallTimeSec));
  EXPECT_SUCCEEDED(RegKey::DeleteValue(cs_key, kRegValueLastUpdateTimeSec));
  EXPECT_FALSE(IsCheckingForUpdates());

  // Old install time, recent update time, old last checked => true.
  EXPECT_SUCCEEDED(cm->SetLastCheckedTime(is_machine_, back_15days_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_36_hours_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_12_hours_sec));
  EXPECT_TRUE(IsCheckingForUpdates());

  // Recent install time, recent update time, recent last checked => true.
  EXPECT_SUCCEEDED(cm->SetLastCheckedTime(is_machine_, back_12_hours_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueInstallTimeSec,
                                    back_12_hours_sec));
  EXPECT_SUCCEEDED(RegKey::SetValue(cs_key,
                                    kRegValueLastUpdateTimeSec,
                                    back_12_hours_sec));
  EXPECT_TRUE(IsCheckingForUpdates());
}

TEST_F(CoreUtilsTest, HasOSUpgraded) {
  const bool is_machine = vista_util::IsUserAdmin();

  // Temporarily redirect the registry elsewhere.
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
  OverrideRegistryHives(kRegistryHiveOverrideRoot);

  // In the absence of a key, assume we haven't upgraded.
  EXPECT_FALSE(HasOSUpgraded());

  // If the key is equal to the current OS, we haven't upgraded.
  EXPECT_SUCCEEDED(app_registry_utils::SetLastOSVersion(is_machine, NULL));
  EXPECT_FALSE(HasOSUpgraded());

  // Build a key with a different major version; we've upgraded.
  OSVERSIONINFOEX oldmajor = {};
  EXPECT_SUCCEEDED(SystemInfo::GetOSVersion(&oldmajor));
  --oldmajor.dwMajorVersion;
  EXPECT_SUCCEEDED(app_registry_utils::SetLastOSVersion(is_machine, &oldmajor));
  EXPECT_TRUE(HasOSUpgraded());

  // Restore the registry redirection.
  RestoreRegistryHives();
  RegKey::DeleteKey(kRegistryHiveOverrideRoot, true);
}

TEST_F(CoreUtilsTest, ShouldRunCore) {
  const bool is_machine = vista_util::IsUserAdmin();
  const TCHAR* reg_update_key(is_machine ? MACHINE_REG_UPDATE :
                                           USER_REG_UPDATE);

  const time64 now = GetCurrentMsTime();
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCoreRun,
                                    now));
  EXPECT_FALSE(ShouldRunCore());

  const time64 nowMinus23Hours = now -
                                 (kSecondsPerDay - kSecondsPerHour) * kMsPerSec;
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCoreRun,
                                    nowMinus23Hours));
  EXPECT_FALSE(ShouldRunCore());

  const time64 nowMinus25PlusHours = now -
      (kSecondsPerDay + kSecondsPerHour + kSecPerMin) * kMsPerSec;
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCoreRun,
                                    nowMinus25PlusHours));
  EXPECT_TRUE(ShouldRunCore());

  EXPECT_SUCCEEDED(RegKey::DeleteValue(reg_update_key,
                                       kRegValueLastCoreRun));
  EXPECT_TRUE(ShouldRunCore());

  const time64 nowPlus2000Days = now +
                                 2000ULL * kSecondsPerDay * kMsPerSec;
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCoreRun,
                                    nowPlus2000Days));
  EXPECT_TRUE(ShouldRunCore());
}

TEST_F(CoreUtilsTest, ShouldRunCodeRed) {
  const bool is_machine = vista_util::IsUserAdmin();
  const TCHAR* reg_update_key(is_machine ? MACHINE_REG_UPDATE: USER_REG_UPDATE);

  const time64 now = GetCurrentMsTime();
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCodeRedCheck,
                                    now));
  EXPECT_FALSE(ShouldRunCodeRed());

  const time64 nowMinus23Hours = now -
                                 (kSecondsPerDay - kSecondsPerHour) * kMsPerSec;
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCodeRedCheck,
                                    nowMinus23Hours));
  EXPECT_FALSE(ShouldRunCodeRed());

  const time64 nowMinus24Hours = now - (kSecondsPerDay + 1) * kMsPerSec;
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCodeRedCheck,
                                    nowMinus24Hours));
  EXPECT_TRUE(ShouldRunCodeRed());

  EXPECT_SUCCEEDED(RegKey::DeleteValue(reg_update_key,
                                       kRegValueLastCodeRedCheck));
  EXPECT_TRUE(ShouldRunCodeRed());

  const time64 nowPlus2000Days = now +
                                 2000ULL * kSecondsPerDay * kMsPerSec;
  EXPECT_SUCCEEDED(RegKey::SetValue(reg_update_key,
                                    kRegValueLastCodeRedCheck,
                                    nowPlus2000Days));
  EXPECT_TRUE(ShouldRunCodeRed());
}

}  // namespace omaha
