// Copyright 2003-2009 Google Inc.
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

#include "omaha/base/synchronized.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

TEST(LLockTest, GetOwner) {
  LLock lock;

  EXPECT_EQ(0, lock.GetOwner());

  EXPECT_TRUE(lock.Lock());
  EXPECT_EQ(::GetCurrentThreadId(), lock.GetOwner());

  EXPECT_TRUE(lock.Unlock());
  EXPECT_EQ(0, lock.GetOwner());
}

TEST(GateTest, WaitAny) {
  const DWORD kTimeout = 100;
  const size_t kFewGates = 10;
  const size_t kNumGates = MAXIMUM_WAIT_OBJECTS + 1;

  Gate gates[kNumGates];
  const Gate* gateptrs[kNumGates] = {};
  for (size_t i = 0; i < kNumGates; ++i) {
    gates[i].Open();
    gateptrs[i] = &gates[i];
  }

  int selectedGate = 0;

  // Test too many or too few gates.
  EXPECT_EQ(E_INVALIDARG,
            Gate::WaitAny(gateptrs, 0, kTimeout, &selectedGate));
  EXPECT_EQ(E_INVALIDARG,
            Gate::WaitAny(gateptrs, kNumGates, kTimeout, &selectedGate));

  // Test all gates open.
  EXPECT_EQ(S_OK,
            Gate::WaitAny(gateptrs, kFewGates, kTimeout, &selectedGate));
  EXPECT_EQ(0, selectedGate);

  // Test all gates closed.
  for (size_t i = 0; i < kNumGates; ++i) {
    gates[i].Close();
  }
  EXPECT_EQ(HRESULT_FROM_WIN32(WAIT_TIMEOUT),
            Gate::WaitAny(gateptrs, kFewGates, kTimeout, &selectedGate));

  // Randomly select a gate to open.
  size_t randomGate = ::rand() % kFewGates;
  gates[randomGate].Open();

  EXPECT_EQ(S_OK,
            Gate::WaitAny(gateptrs, kFewGates, kTimeout, &selectedGate));
  EXPECT_EQ(randomGate, selectedGate);
}

TEST(GateTest, WaitAll) {
  const DWORD kTimeout = 100;
  const size_t kFewGates = 10;
  const size_t kNumGates = MAXIMUM_WAIT_OBJECTS + 1;

  Gate gates[kNumGates];
  const Gate* gateptrs[kNumGates] = {};
  for (size_t i = 0; i < kNumGates; ++i) {
    gates[i].Open();
    gateptrs[i] = &gates[i];
  }

  // Test too many or too few gates.
  EXPECT_EQ(E_INVALIDARG, Gate::WaitAll(gateptrs, 0, kTimeout));
  EXPECT_EQ(E_INVALIDARG, Gate::WaitAll(gateptrs, kNumGates, kTimeout));

  // Test all gates open.
  EXPECT_EQ(S_OK, Gate::WaitAll(gateptrs, kFewGates, kTimeout));

  // Test all gates closed.
  for (size_t i = 0; i < kNumGates; ++i) {
    gates[i].Close();
  }
  EXPECT_EQ(HRESULT_FROM_WIN32(WAIT_TIMEOUT),
            Gate::WaitAll(gateptrs, kFewGates, kTimeout));

  // Open one gate.  Should still time out.
  gates[0].Open();
  EXPECT_EQ(HRESULT_FROM_WIN32(WAIT_TIMEOUT),
            Gate::WaitAll(gateptrs, kFewGates, kTimeout));
}

}  // namespace omaha

