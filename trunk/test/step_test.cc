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

#include "omaha/test/step_test.h"

const TCHAR *StepTestBase::RESUME_EVENT_TEMPLATE = _T("R %s");
const TCHAR *StepTestBase::STEP_REACHED_EVENT_TEMPLATE = _T("SR %s");
const TCHAR *StepTestBase::STATE_REG_KEY_TEMPLATE = _T("SOFTWARE\\GAT %s");
const TCHAR *StepTestBase::STATE_REG_VALUE_NAME = _T("state");

StepTestBase::StepTestBase(const TCHAR*) : step_reached_event_(NULL),
resume_event_(NULL), state_key_(NULL) {
}

void StepTestBase::Terminate() {
  CloseHandle(step_reached_event_);
  CloseHandle(resume_event_);
  RegCloseKey(state_key_);
}

StepTestBase::~StepTestBase() {
  Terminate();
}

StepTestStepper::StepTestStepper(const TCHAR* name) :
  StepTestBase(name),
  is_someone_listening_(false) {
  CString namebuf;

  // A "broadcaster" is a program that notifies the "listener" program that
  // the broadcaster has reached an interesting point in its execution, and
  // the listener should check state.
  //
  // Results will be unpredictable if two broadcasters are running at the
  // same time. However, since a broadcaster will only open the resume event
  // and not create it, we shouldn't see any problems where two broadcasters
  // deadlock each other.
  namebuf.Format(RESUME_EVENT_TEMPLATE, name);
  resume_event_ = OpenEvent(EVENT_ALL_ACCESS, FALSE, namebuf);
  if (resume_event_ != NULL) {
    is_someone_listening_ = true;

    namebuf.Format(STEP_REACHED_EVENT_TEMPLATE, name);
    step_reached_event_ = OpenEvent(EVENT_ALL_ACCESS, FALSE, namebuf);

    namebuf.Format(STATE_REG_KEY_TEMPLATE, name);
    RegCreateKeyEx(HKEY_CURRENT_USER, namebuf, 0, NULL, REG_OPTION_VOLATILE,
      KEY_READ | KEY_WRITE, NULL, &state_key_, NULL);
  } else {
    Terminate();
  }
}

StepTestStepper::~StepTestStepper() {
  Terminate();
}

void StepTestStepper::Step(DWORD step) {
  if (!is_someone_listening_) {
    return;
  }
  RegSetValueEx(state_key_, STATE_REG_VALUE_NAME, 0, REG_DWORD,
    reinterpret_cast<BYTE*>(&step), sizeof(step));
  SetEvent(step_reached_event_);
  WaitForSingleObject(resume_event_, INFINITE);
}

StepTestWatcher::StepTestWatcher(const TCHAR* name) : StepTestBase(name),
  name_(name) {
}

StepTestWatcher::~StepTestWatcher() {
  Terminate();
}

void StepTestWatcher::Go() {
  DWORD step = 0;
  bool done = false;

  PrintIntroduction();

  printf("Verifying preconditions...\r\n");
  bool passed = VerifyStep(0, &done);
  if (passed) {
    _tprintf(_T("Preconditions OK.\r\n"));
  } else {
    _tprintf(_T("*** Preconditions FAILED. ***\r\n"));
  }

  CString namebuf;

  namebuf.Format(RESUME_EVENT_TEMPLATE, name_);
  resume_event_ = CreateEvent(NULL, FALSE, FALSE, namebuf);
  namebuf.Format(STEP_REACHED_EVENT_TEMPLATE, name_);
  step_reached_event_ = CreateEvent(NULL, FALSE, FALSE, namebuf);

  namebuf.Format(STATE_REG_KEY_TEMPLATE, name_);
  RegCreateKeyEx(HKEY_CURRENT_USER, namebuf, 0, NULL, REG_OPTION_VOLATILE,
    KEY_READ, NULL, &state_key_, NULL);

  printf("Begin test now.\r\n");

  done = false;
  while (!done) {
    WaitForSingleObject(step_reached_event_, INFINITE);
    DWORD step_size = sizeof(step);
    RegQueryValueEx(state_key_, STATE_REG_VALUE_NAME, NULL, NULL,
      reinterpret_cast<BYTE*>(&step), &step_size);
    printf("Testing step %d.\r\n", step);
    passed = VerifyStep(step, &done);
    if (!passed) {
      _tprintf(_T("\r\n\r\n*** TEST FAILURE: Step %d. ***\r\nr\n"), step);
    }
    SetEvent(resume_event_);
  }

  PrintConclusion();
}

bool StepTestWatcher::RegistryKeyExists(HKEY root, const TCHAR* name) {
  HKEY key;
  LONG result = RegOpenKeyEx(root, name, 0, KEY_READ, &key);
  if (ERROR_SUCCESS == result) {
    RegCloseKey(key);
    return true;
  }
  return false;
}

bool StepTestWatcher::RegistryValueExists(HKEY root, const TCHAR* key_name,
                                          const TCHAR* value_name) {
  bool result = false;
  HKEY key;
  LONG reg_result = RegOpenKeyEx(root, key_name, 0, KEY_READ, &key);
  if (ERROR_SUCCESS == reg_result) {
    DWORD dummy_size = 0;
    reg_result = RegQueryValueEx(key, value_name, NULL, NULL, NULL,
      &dummy_size);
    if (reg_result == ERROR_SUCCESS && dummy_size > 0) {
      result = true;
    }
    RegCloseKey(key);
  }
  return result;
}
