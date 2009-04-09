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
# PLEASE KEEP THE LIST ALPHABETIZED BY APP NAME.
EXTERNAL_APPS = [
    ('Gears',
     '{283EAF47-8817-4C2B-A801-AD1FADFB7BAA}',
     ' Check http://gears.google.com/.'),
    ('Google Apps',
     '{C4D65027-B96A-49A5-B13C-4E7FAFD2FB7B}',
     ''),
    ('Google Chrome',
     '{8A69D345-D564-463C-AFF1-A69D9E530F96}',
     ' Check http://www.google.com/chrome/.'),
    ('Google Earth',
     '{74AF07D8-FB8F-4D51-8AC7-927721D56EBB}',
     ' Check http://earth.google.com/.'),
    ('Google Earth (per-user install)',
     '{0A52903D-0FBF-439A-93E4-CB609A2F63DB}',
     ' Check http://earth.google.com/.'),
    ('Google Earth Plugin',
     '{2BF2CA35-CCAF-4E58-BAB7-4163BFA03B88}',
     ' Check http://code.google.com/apis/earth/.'),
    ('Google Earth Pro',
     '{65E60E95-0DE9-43FF-9F3F-4F7D2DFF04B5}',
     ' Check http://earth.google.com/enterprise/earth_pro.html.'),
    ('Google Email Uploader',
     '{84F41014-78F2-4EBF-AF9B-8D7D12FCC37B}',
     ' Check http://mail.google.com/mail/help/email_uploader.html.'),
    ('Google Talk Labs Edition',
     '{7C9D2019-25AD-4F9B-B4C4-F0F537A9626E}',
     ' Check http://www.google.com/talk/labsedition/.'),
    ('Google Talk Plugin (Voice and Video Chat)',
     '{D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}',
     ' Check http://mail.google.com/videochat.'),
    ]
