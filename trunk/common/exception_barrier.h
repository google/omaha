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
#ifndef OMAHA_COMMON_EXCEPTION_BARRIER_H_
#define OMAHA_COMMON_EXCEPTION_BARRIER_H_

#include <windows.h>

/// This is the type dictated for an exception handler by the platform ABI
/// @see _except_handler in excpt.h
typedef EXCEPTION_DISPOSITION (__cdecl *ExceptionHandlerFunc)(
                                struct _EXCEPTION_RECORD *exception_record,
                                void * establisher_frame,
                                struct _CONTEXT *context,
                                void * reserved);

/// The type of an exception record in the exception handler chain
struct EXCEPTION_REGISTRATION {
  EXCEPTION_REGISTRATION *prev;
  ExceptionHandlerFunc  handler;
};

/// This is our raw exception handler, it must be declared extern "C" to
/// match up with the SAFESEH declaration in our corresponding ASM file
extern "C" EXCEPTION_DISPOSITION __cdecl
ExceptionBarrierHandler(struct _EXCEPTION_RECORD *exception_record,
                        void * establisher_frame,
                        struct _CONTEXT *context,
                        void * reserved);

/// An exception barrier is used to report exceptions that pass through
/// a boundary where exceptions shouldn't pass, such as e.g. COM interface
/// boundaries.
/// This is handy for any kind of plugin code, where if the exception passes
/// through unhindered, it'll either be swallowed by an SEH exception handler
/// above us on the stack, or be reported as an unhandled exception for
/// the application hosting the plugin code.
///
/// To use this class, simply instantiate an ExceptionBarrier just inside
/// the code boundary, like this:
/// @code
/// HRESULT SomeObject::SomeCOMMethod(...) {
///   ExceptionBarrier report_crashes;
///
///   ... other code here ...
/// }
/// @endcode
class ExceptionBarrier {
 public:
  /// Register the barrier in the SEH chain
  ExceptionBarrier();

  /// And unregister on destruction
  ~ExceptionBarrier();

  /// Signature of the handler function which gets notified when
  /// an exception propagates through a barrier.
  typedef void (CALLBACK *ExceptionHandler)(EXCEPTION_POINTERS *ptrs);

  /// @name Accessors
  /// @{
  static void set_handler(ExceptionHandler handler) { s_handler_ = handler; }
  static ExceptionHandler handler() { return s_handler_; }
  /// @}

 private:
  /// Our SEH frame
  EXCEPTION_REGISTRATION registration_;

  /// The function that gets invoked if an exception
  /// propagates through a barrier
  /// TODO(omaha): how can this be statically parametrized?
  static ExceptionHandler s_handler_;
};

/// @name These are implemented in the associated .asm file
/// @{
extern "C" void WINAPI RegisterExceptionRecord(
                          EXCEPTION_REGISTRATION *registration,
                          ExceptionHandlerFunc func);
extern "C" void WINAPI UnregisterExceptionRecord(
                          EXCEPTION_REGISTRATION *registration);
/// @}


inline ExceptionBarrier::ExceptionBarrier() {
  RegisterExceptionRecord(&registration_, ExceptionBarrierHandler);
}

inline ExceptionBarrier::~ExceptionBarrier() {
  // TODO(omaha): I don't think it's safe to unregister after an exception
  //          has taken place???
  UnregisterExceptionRecord(&registration_);
}

#endif  // OMAHA_COMMON_EXCEPTION_BARRIER_H_
