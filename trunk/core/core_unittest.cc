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


#include "omaha/common/const_object_names.h"
#include "omaha/common/error.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/thread.h"
#include "omaha/common/utils.h"
#include "omaha/core/core.h"
#include "omaha/testing/unit_test.h"

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
    core.Main(is_machine_, true);         // Run the crash handler.
  }

  bool is_machine_;
  DISALLOW_EVIL_CONSTRUCTORS(CoreRunner);
};

}  // namespace

class CoreTest : public testing::Test {
 public:
  CoreTest() : is_machine_(false) {}

  virtual void SetUp() {
    ASSERT_HRESULT_SUCCEEDED(IsSystemProcess(&is_machine_));

    NamedObjectAttributes attr;
    GetNamedObjectAttributes(kShutdownEvent, is_machine_, &attr);
    reset(shutdown_event_, ::CreateEvent(&attr.sa, true, false, attr.name));
    ASSERT_TRUE(shutdown_event_);
  }

  virtual void TearDown() {
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
  if (thread.Running()) {
    thread.Terminate(-1);
  }
  EXPECT_HRESULT_SUCCEEDED(ResetShutdownEvent());
}

}  // namespace omaha

