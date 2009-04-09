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

// PP_STRINGIZE - expands arguments before stringizing
// PP_STRINGIZE(PP_CAT(a,b)) => "ab"

#define PP_STRINGIZE(text)   PP_STRINGIZE_A((text))
#define PP_STRINGIZE_A(arg)  PP_STRINGIZE_B ## (arg)
#define PP_STRINGIZE_B(arg)  PP_STRINGIZE_I ## arg
#define PP_STRINGIZE_I(text) #text

// T_PP_STRINGIZE - expands arguments before stringizing
// T_PP_STRINGIZE(PP_CAT(a,b)) => _T("ab")

#define T_PP_STRINGIZE(text)   T_PP_STRINGIZE_A((text))
#define T_PP_STRINGIZE_A(arg)  T_PP_STRINGIZE_B ## (arg)
#define T_PP_STRINGIZE_B(arg)  T_PP_STRINGIZE_I ## arg
#define T_PP_STRINGIZE_I(text) _T(#text)

// PP_CAT - concatenates arguments after they have been expanded
// PP_CAT(x, PP_CAT(y,z)) => xyz
#define PP_CAT(a,b)   PP_CAT_I(a,b)
#define PP_CAT_I(a,b) PP_CAT_J(a##b)
#define PP_CAT_J(arg) arg
