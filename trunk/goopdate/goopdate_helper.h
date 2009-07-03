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

#ifndef OMAHA_GOOPDATE_GOOPDATE_HELPER_H__
#define OMAHA_GOOPDATE_GOOPDATE_HELPER_H__

#include <windows.h>


namespace omaha {

class JobObserver;
class Ping;
struct CommandLineArgs;

// Sends the GoopdateStartedPing, calls setup to finish setup and sends
// the finish setup complete ping.
HRESULT FinishGoogleUpdateInstall(const CommandLineArgs& args,
                                  bool is_machine,
                                  bool is_self_update,
                                  Ping* ping,
                                  JobObserver* job_observer);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_HELPER_H__

