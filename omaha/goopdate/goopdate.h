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
#include <memory>

#include "base/basictypes.h"
#include "omaha/base/thread_pool.h"

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

  // Gets the singleton instance of the class.
  static Goopdate& Instance();

  // Runs the entry point for the application.
  HRESULT Main(HINSTANCE instance, const TCHAR* cmd_line, int cmd_show);

  HRESULT QueueUserWorkItem(std::unique_ptr<UserWorkItem> work_item,
                            DWORD coinit_flags,
                            uint32 flags);

  void Stop();

  bool is_local_system() const;
  CommandLineArgs args() const;

 private:
  static Goopdate* instance_;

  // Uses pimpl idiom to minimize dependencies on implementation details.
  std::unique_ptr<detail::GoopdateImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(Goopdate);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_H__

