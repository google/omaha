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

#ifndef OMAHA_GOOPDATE_EXTRA_ARGS_PARSER_H__
#define OMAHA_GOOPDATE_EXTRA_ARGS_PARSER_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/goopdate/command_line.h"

namespace omaha {

// This class handles tokenizing and parsing the ExtraArgs portion of the
// command line.
// The format of the extra arguments is as follows:
// appguid={}&.....appguid={}....
// appguid has to be the first value of the extra arguments.
// Each appguid defines one product.
class ExtraArgsParser {
 public:
  ExtraArgsParser() : first_app_(false) {}

  // Parses args->extra_args into other values of args.
  HRESULT Parse(const TCHAR* extra_args,
                const TCHAR* app_args,
                CommandLineExtraArgs* args);

 private:
  HRESULT ParseAppArgs(const TCHAR* app_args, CommandLineExtraArgs* args);

  // Performs validation against extra_args and if it's valid, stores the
  // extra_args value into args->extra_args.
  HRESULT Validate(const TCHAR* extra_args);
  HRESULT HandleToken(const CString& token, CommandLineExtraArgs* args);
  HRESULT HandleAppArgsToken(const CString& token,
                             CommandLineExtraArgs* args,
                             int* cur_app_args_index);

  CommandLineAppArgs cur_extra_app_args_;
  bool first_app_;

  DISALLOW_EVIL_CONSTRUCTORS(ExtraArgsParser);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_EXTRA_ARGS_PARSER_H__

