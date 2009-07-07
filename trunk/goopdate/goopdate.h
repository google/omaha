// Copyright 2008-2009 Google Inc.
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


#ifndef OMAHA_GOOPDATE_GOOPDATE_H__
#define OMAHA_GOOPDATE_GOOPDATE_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace omaha {

namespace detail {

class GoopdateImpl;

}  // namespace detail

struct CommandLineArgs;
class ConfigManager;
class ResourceManager;

class Goopdate {
 public:
  explicit Goopdate(bool is_local_system);
  ~Goopdate();

  // Runs the entry point for the application.
  HRESULT Main(HINSTANCE instance, const TCHAR* cmd_line, int cmd_show);

  bool is_local_system() const;
  CommandLineArgs args() const;
  CString cmd_line() const;

 private:
  // Uses pimpl idiom to minimize dependencies on implementation details.
  scoped_ptr<detail::GoopdateImpl> impl_;

  DISALLOW_EVIL_CONSTRUCTORS(Goopdate);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_H__

