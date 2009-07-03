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
//
// TODO(omaha): move the code to common.

#ifndef OMAHA_GOOPDATE_PROGRAM_INSTANCE_H__
#define OMAHA_GOOPDATE_PROGRAM_INSTANCE_H__

#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/constants.h"
#include "omaha/third_party/smartany/auto_any.h"

namespace omaha {

  // Helps limit the number of instances of a program. The class itself does not
  // limit the number of instances. The calling code is expected to take action
  // based on the return of EnsureSingleInstance function.
  class ProgramInstance {
   public:
    explicit ProgramInstance(const TCHAR* mutex_name)
        : mutex_name_(mutex_name) {}
    virtual ~ProgramInstance() {}
    bool EnsureSingleInstance();
   private:
    bool CheckSingleInstance();
    CString mutex_name_;
    auto_mutex mutex_;
    DISALLOW_EVIL_CONSTRUCTORS(ProgramInstance);
  };

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_PROGRAM_INSTANCE_H__
