// Copyright 2006-2009 Google Inc.
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
//
// A class to make it easy to tag exception propagation boundaries and
// get crash reports of exceptions that pass over same.
#include "omaha/common/exception_barrier.h"

enum {
  // Flag set by exception handling machinery when unwinding
  EH_UNWINDING = 0x00000002
};

// TODO(omaha): How to make this statically parameterizable?
ExceptionBarrier::ExceptionHandler ExceptionBarrier::s_handler_ = NULL;

// This function must be extern "C" to match up with the SAFESEH
// declaration in our corresponding ASM file
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierHandler(struct _EXCEPTION_RECORD *exception_record,
                        void * establisher_frame,
                        struct _CONTEXT *context,
                        void * reserved) {
  establisher_frame;  // unreferenced formal parameter
  reserved;
  if (!(exception_record->ExceptionFlags & EH_UNWINDING)) {
    // When the exception is really propagating through us, we'd like to be
    // called before the state of the program has been modified by the stack
    // unwinding. In the absence of an exception handler, the unhandled
    // exception filter gets called between the first chance and the second
    // chance exceptions, so Windows pops either the JIT debugger or WER UI.
    // This is not desirable in most of the cases.
    ExceptionBarrier::ExceptionHandler handler = ExceptionBarrier::handler();
    if (handler) {
      EXCEPTION_POINTERS ptrs = { exception_record, context };

      handler(&ptrs);
    }
  }

  return ExceptionContinueSearch;
}
