// Copyright 2006-2013 Google Inc.
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
// Manages intended changes to an environment block, and applies these intended
// changes to a given environment block to produce a new one.

#ifndef OMAHA_BASE_ENVIRONMENT_BLOCK_MODIFIER_H_
#define OMAHA_BASE_ENVIRONMENT_BLOCK_MODIFIER_H_

#include <windows.h>
#include <tchar.h>
#include <atlstr.h>
#include <map>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

class EnvironmentBlockModifier {
 public:
  EnvironmentBlockModifier();

  // Specifies intent to override environment variable |name| with |val|.
  // If |val| is empty, then the intent is to unset the variable.
  void SetVar(const CString& name, const CString& val);

  // Returns true if there are no intended changes.
  bool IsEmpty() const;

  // Given an environment block |env_block|, e.g.,
  //   name1=value1\0name2=value2\0name3=value3\0\0
  // returns a new environment block consisting of |env_block| data with the
  // intended modifications via |out|.
  // |env_block| strings are assumed to be sorted alphabetically by names
  // (case-insensitive). This property is preserved in the returned value.
  // |env_block| strings with names starting with '=' are copied as-is.
  void Create(const TCHAR* env_block, std::vector<TCHAR>* out) const;

  // Reads the environment block from |user_token|, performs the intended
  // modifications, and returns the result via |out|.
  // Returns true on success.
  bool CreateForUser(HANDLE user_token, std::vector<TCHAR>* out) const;

  // Reads the environment block for the current user, performs the intended
  // modifications, and returns the result via |out|.
  // Returns true on success.
  bool CreateForCurrentUser(std::vector<TCHAR>* out) const;

 private:
  struct CompareCStringCaseInsensitive {
    bool operator()(const CString& s1, const CString& s2) const;
  };

  std::map<CString, CString, CompareCStringCaseInsensitive> env_var_;

  DISALLOW_COPY_AND_ASSIGN(EnvironmentBlockModifier);
};

}  // namespace omaha

#endif  // OMAHA_BASE_ENVIRONMENT_BLOCK_MODIFIER_H_
