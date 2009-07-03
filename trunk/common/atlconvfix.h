// Copyright 2004-2009 Google Inc.
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
// atlconvfix.h
//
// This file is included in the precompile headers.
// Do not include the base/basictypes.h here.

#ifndef OMAHA_COMMON_ATLCONVFIX_H_
#define OMAHA_COMMON_ATLCONVFIX_H_

#ifndef DISALLOW_EVIL_CONSTRUCTORS
// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_EVIL_CONSTRUCTORS(TypeName)    \
  TypeName(const TypeName&);                    \
  void operator=(const TypeName&)
#endif

// These use alloca which can be dangerous,
// so we don't allow them to be used.  Use
// CA2[C]W, etc. instead.
#undef A2W
#undef W2A
#undef A2W_EX
#undef W2A_EX
#undef USES_CONVERSION

#ifdef DEBUG
// The DestroyBuffer template and the macros following it are
// all there to ensure that when the string classes get destroyed,
// so does the string that they return since it is no longer valid.
// Without them it is very easy to make simple and hard to catch mistakes
// when using the atl C*2[C]* string converstion classes.

// In non-debug code, the atl string macros do not need to be destroyed,
// so they aren't wrapped there.

template <int buffer_length, template <int buffer_length> class ConversionClass,
          typename StringType>
class DestroyBuffer : public ConversionClass<buffer_length> {
 public:
  DestroyBuffer(StringType string) : ConversionClass<buffer_length>(string) {
  }

  DestroyBuffer(StringType string, UINT code_page) :
      ConversionClass<buffer_length>(string, code_page) {
  }

  ~DestroyBuffer() {
    memset(m_szBuffer, 0xdd, sizeof(m_szBuffer));
  }

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DestroyBuffer);
};

#define DECLARED_DESTROY_NAME(base_type) DestroyBuffer##base_type
#define DECLARE_DESTROY_TYPES(base_type, string_type) \
  template <int buffer_length = 128> \
  class DECLARED_DESTROY_NAME(base_type##EX) : \
      public DestroyBuffer<buffer_length, base_type##EX, string_type> { \
   public: \
    DECLARED_DESTROY_NAME(base_type##EX)(string_type string) : \
      DestroyBuffer<buffer_length, base_type##EX, string_type>(string) {  \
    } \
    DECLARED_DESTROY_NAME(base_type##EX)(string_type string, \
                                         UINT code_page) : \
      DestroyBuffer<buffer_length, base_type##EX, string_type>(string, \
                                                               code_page) { \
    } \
   private: \
    DISALLOW_EVIL_CONSTRUCTORS(DECLARED_DESTROY_NAME(base_type##EX)); \
  }; \
  typedef DECLARED_DESTROY_NAME(base_type##EX)<> \
      DECLARED_DESTROY_NAME(base_type)

DECLARE_DESTROY_TYPES(CW2W, LPCWSTR);
DECLARE_DESTROY_TYPES(CW2A, LPCWSTR);
DECLARE_DESTROY_TYPES(CA2W, LPCSTR);
DECLARE_DESTROY_TYPES(CA2A, LPCSTR);

#define CW2WEX DECLARED_DESTROY_NAME(CW2WEX)
#define CW2AEX DECLARED_DESTROY_NAME(CW2AEX)
#define CA2WEX DECLARED_DESTROY_NAME(CA2WEX)
#define CA2AEX DECLARED_DESTROY_NAME(CA2AEX)
#define CW2W   DECLARED_DESTROY_NAME(CW2W)
#define CW2A   DECLARED_DESTROY_NAME(CW2A)
#define CA2W   DECLARED_DESTROY_NAME(CA2W)
#define CA2A   DECLARED_DESTROY_NAME(CA2A)
#endif // DEBUG

#endif // OMAHA_COMMON_ATLCONVFIX_H_
