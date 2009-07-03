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

#include <windows.h>
#include <tchar.h>

// This file implements a tiny program that puts up a MessageBox and exits.
// It's useful for generating test installation targets.
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  MessageBox(NULL, _T("I am bar"), _T("bar"), MB_OK | MB_ICONINFORMATION);
  return 0;
}
