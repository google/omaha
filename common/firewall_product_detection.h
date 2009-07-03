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

#ifndef OMAHA_COMMON_FIREWALL_PRODUCT_DETECTION_H_
#define OMAHA_COMMON_FIREWALL_PRODUCT_DETECTION_H_

#include <windows.h>
#include <atlstr.h>

namespace omaha {

namespace firewall_detection {

// Detects if the computer is running a software firewall.
HRESULT Detect(CString* name, CString* version);

}  // namespace firewall_detection

}  // namespace omaha

#endif  // OMAHA_COMMON_FIREWALL_PRODUCT_DETECTION_H_
