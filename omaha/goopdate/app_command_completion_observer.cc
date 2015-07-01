// Copyright 2013 Google Inc.
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

#include "omaha/goopdate/app_command_completion_observer.h"

#include <algorithm>
#include "base/scoped_ptr.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/string.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/ping.h"
#include "omaha/goopdate/app_command_delegate.h"

namespace omaha {

AppCommandCompletionObserver::AppCommandCompletionObserver()
    : exit_code_(MAXDWORD),
      status_(COMMAND_STATUS_RUNNING) {
}

HRESULT AppCommandCompletionObserver::Init(
    HANDLE process,
    HANDLE output,
    const shared_ptr<AppCommandDelegate>& delegate) {
  reset(process_, process);
  reset(output_pipe_, output);
  delegate_ = delegate;

  CComPtr<AppCommandCompletionObserver> for_other_thread(this);
  void* context = reinterpret_cast<void *>(
      static_cast<AppCommandCompletionObserver*>(for_other_thread));

  reset(thread_, ::CreateThread(NULL, 0, ThreadStart, context, 0, NULL));

  if (valid(thread_)) {
    // The thread will manage this reference.
    for_other_thread.Detach();
    return S_OK;
  }

  HRESULT hr = HRESULTFromLastError();
  CORE_LOG(LE, (_T("[failed to start wait thread for app command ")
                _T("process exit]") _T("[0x%08x]"), hr));
  return hr;
}

AppCommandCompletionObserver::~AppCommandCompletionObserver() {
}

HRESULT AppCommandCompletionObserver::Start(
    HANDLE process,
    HANDLE output,
    const shared_ptr<AppCommandDelegate>& delegate,
    AppCommandCompletionObserver** observer) {
  ASSERT1(process);
  ASSERT1(observer);

  scoped_process process_handle(process);
  scoped_handle output_handle(output);
  process = NULL;
  output = NULL;

  CComPtr<AppCommandCompletionObserver> local_observer;
  {
    scoped_ptr<CComObject<AppCommandCompletionObserver> > new_object;
    HRESULT hr = CComObject<AppCommandCompletionObserver>::CreateInstance(
        address(new_object));
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[failed to create AppCommandCompletionObserver]"),
                    _T("[0x%08x]"), hr));
      return hr;
    }
    local_observer = new_object.release();
  }

  HRESULT hr = local_observer->Init(release(process_handle),
                                    release(output_handle),
                                    delegate);
  if (SUCCEEDED(hr)) {
    *observer = local_observer.Detach();
  }

  return hr;
}

CString AppCommandCompletionObserver::GetOutput() {
  AutoSync lock(lock_);
  return output_;
}

AppCommandStatus AppCommandCompletionObserver::GetStatus() {
  AutoSync lock(lock_);
  return status_;
}

DWORD AppCommandCompletionObserver::GetExitCode() {
  AutoSync lock(lock_);
  return exit_code_;
}

void AppCommandCompletionObserver::CaptureOutput() {
  if (!output_pipe_) {
    return;
  }

  scoped_handle local_output(release(output_pipe_));

  // Read output from the child process's pipe for STDOUT
  const int kBufferSize = 65536;
  char buffer[kBufferSize + sizeof(TCHAR)];
  int total_read = 0;

  for (;;) {
    DWORD bytes_read = 0;
    BOOL success = ::ReadFile(get(local_output), buffer + total_read,
                              kBufferSize - total_read,
                              &bytes_read, NULL);
    total_read += bytes_read;
    if (!success || bytes_read == 0 || total_read == kBufferSize) {
      break;
    }
  }

  int unicode_tests = IS_TEXT_UNICODE_UNICODE_MASK;
  bool is_unicode = (IsTextUnicode(buffer, total_read, &unicode_tests) != 0);

  {
    AutoSync lock(lock_);
    if (is_unicode) {
      *reinterpret_cast<TCHAR*>(buffer + total_read) = 0;
      output_ = reinterpret_cast<TCHAR*>(buffer);
    } else {
      buffer[total_read] = 0;
      output_ = buffer;
    }
  }

  return;
}

HRESULT AppCommandCompletionObserver::WaitForProcessExit() {
  HRESULT hr = S_OK;

  DWORD wait_result = ::WaitForSingleObject(get(process_), INFINITE);
  DWORD exit_code = MAXDWORD;

  if (wait_result == WAIT_TIMEOUT) {
    hr = GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT;
  } else if (wait_result == WAIT_FAILED) {
    hr = HRESULTFromLastError();
  } else {
    ASSERT1(wait_result == WAIT_OBJECT_0);

    if (wait_result != WAIT_OBJECT_0) {
      hr = E_UNEXPECTED;
    } else if (!::GetExitCodeProcess(get(process_), &exit_code)) {
      hr = HRESULTFromLastError();
    }
  }

  {
    AutoSync lock(lock_);
    if (FAILED(hr)) {
      status_ = COMMAND_STATUS_ERROR;
    } else {
      status_ = COMMAND_STATUS_COMPLETE;
      exit_code_ = exit_code;
    }
  }

  return hr;
}

bool AppCommandCompletionObserver::Join(int timeoutMs) {
  if (valid(thread_)) {
    return WAIT_OBJECT_0 == WaitForSingleObject(get(thread_), timeoutMs);
  }
  return false;
}

void AppCommandCompletionObserver::Execute() {
  CaptureOutput();
  HRESULT hr = WaitForProcessExit();

  if (!delegate_.get()) {
    return;
  }

  if (FAILED(hr)) {
    delegate_->OnObservationFailure(hr);
  } else {
    delegate_->OnCommandCompletion(exit_code_);
  }
}

DWORD WINAPI AppCommandCompletionObserver::ThreadStart(void* parameter) {
  scoped_co_init init_com_apt(COINIT_MULTITHREADED);
  HRESULT hr = init_com_apt.hresult();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[init_com_apt failed][0x%x]"), hr));
    return 0;
  }

  // The ref-count was incremented in the parent thread, and this thread takes
  // responsibility to release it.
  CComPtr<AppCommandCompletionObserver> instance;
  instance.Attach(reinterpret_cast<AppCommandCompletionObserver*>(parameter));
  instance->Execute();
  return 0;
}

}  // namespace omaha
