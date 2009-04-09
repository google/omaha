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

#ifndef OMAHA_NET_NET_UTILS_H__
#define OMAHA_NET_NET_UTILS_H__

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

namespace omaha {

// Returns true if the machine is connected to the LAN/WAN network. This
// does not mean the machine is able to access the Internet. When the function
// can't determine the connection state, it assumes the machine is connected
// and it returns true as well.
bool IsMachineConnectedToNetwork();

// Converts a buffer or a vector to a string for logging purposes.
// Non-printable characters are converted to '.'.
CString BufferToPrintableString(const void* buffer, size_t length);
CString VectorToPrintableString(const std::vector<uint8>& response);

}  // namespace omaha

#endif  // OMAHA_NET_NET_UTILS_H__

