// Copyright 2005-2009 Google Inc.
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

#ifndef OMAHA_COMMON_SCOPED_CURRENT_DIRECTORY_H_
#define OMAHA_COMMON_SCOPED_CURRENT_DIRECTORY_H_

namespace omaha {

// Utility class to "scope" the setting of a current directory, and restore
// to the previous current directory when leaving the scope.  Handles the
// case where we don't actually have a new current directory to switch to
// (thus the parameter is the empty string).

class scoped_current_directory {
  public:
    explicit scoped_current_directory(const TCHAR* new_directory) {
      *was_directory_ = _T('\0');
      if (new_directory && *new_directory) {
        ::GetCurrentDirectory(arraysize(was_directory_), was_directory_);
        ::SetCurrentDirectory(new_directory);
      }
    }
    ~scoped_current_directory() {
      if (*was_directory_) {
        ::SetCurrentDirectory(was_directory_);
      }
    }
  private:
    TCHAR was_directory_[MAX_PATH];
};

}  // namespace omaha

#endif // OMAHA_COMMON_SCOPED_CURRENT_DIRECTORY_H_
