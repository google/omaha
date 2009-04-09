#!/usr/bin/python2.4
#
# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ========================================================================

"""Contains the list of supported apps that have been publicly announced.

This file is used in the creation of GoogleUpdate.adm.
"""

# Specifies the list of supported apps that have been publicly announced.
# The list is used to create app-specific entries in the Group Policy template.
# Each element of the list is a tuple of:
# (app name, app ID, optional string to append to the auto-update explanation).
# The auto-update explanation should start with a space or double new line
# (\n\n)
EXTERNAL_APPS = [
      ('Google Chrome',
       '{8A69D345-D564-463C-AFF1-A69D9E530F96}',
       ' Check http://www.google.com/chrome/.'),
    ]
