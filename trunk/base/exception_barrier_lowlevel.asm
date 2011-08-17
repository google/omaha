; Copyright 2006-2009 Google Inc.
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;      http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.
; ========================================================================
;
; Tag the exception handler as an SEH handler in case the executable
; is linked with /SAFESEH (which is the default).
;
; MASM 8.0 inserts an additional leading underscore in front of names
; and this is an attempted fix until we understand why.
IF @version LT 800
_ExceptionBarrierHandler PROTO
.SAFESEH _ExceptionBarrierHandler
ELSE
ExceptionBarrierHandler PROTO
.SAFESEH ExceptionBarrierHandler
ENDIF

.586
.MODEL FLAT, STDCALL
ASSUME FS:NOTHING
.CODE

; extern "C" void WINAPI RegisterExceptionRecord(
;                          EXCEPTION_REGISTRATION *registration,
;                          ExceptionHandlerFunc func);
RegisterExceptionRecord PROC registration:DWORD, func:DWORD
OPTION PROLOGUE:None
OPTION EPILOGUE:None
  mov   edx, DWORD PTR [esp + 4]  ; edx is registration
  mov   eax, DWORD PTR [esp + 8] ; eax is func
  mov   DWORD PTR [edx + 4], eax
  mov   eax, FS:[0]
  mov   DWORD PTR [edx], eax
  mov   FS:[0], edx
  ret   8

RegisterExceptionRecord ENDP

; extern "C" void UnregisterExceptionRecord(
;                           EXCEPTION_REGISTRATION *registration);
UnregisterExceptionRecord PROC registration:DWORD
OPTION PROLOGUE:None
OPTION EPILOGUE:None

  mov   edx, DWORD PTR [esp + 4]
  mov   eax, [edx]
  mov   FS:[0], eax
  ret   4

UnregisterExceptionRecord ENDP

END
