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
    ('Google Advertising Cookie Opt-out Plugin',
     '{ADDE8406-A0F3-4AC2-8878-ADC0BD37BD86}',
     ' Check http://www.google.com/ads/preferences/plugin/.'),
    ('Google Apps',
     '{C4D65027-B96A-49A5-B13C-4E7FAFD2FB7B}',
     ''),
    ('Google Apps Sync for Microsoft Outlook',
     '{BEBCAD10-F1BC-4F92-B4A7-9E2545C809ED}',
     ' Check http://tools.google.com/dlpage/gappssync/.'),
    ('Google Chrome',
     '{8A69D345-D564-463C-AFF1-A69D9E530F96}',
     ' Check http://www.google.com/chrome/.'),
    ('Google Chrome Frame',
     '{8BA986DA-5100-405E-AA35-86F34A02ACBF}',
     ' Check http://www.google.com/chromeframe/.'),
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
    ('O3D',
     '{70308795-045C-42DA-8F4E-D452381A7459}',
     ' Check http://code.google.com/apis/o3d/.'),
    ('O3D Extras',
     '{34B2805D-C72C-4F81-AED5-5A22D1E092F1}',
     ''),
    # IMEs.
    # All are transliteration IMEs unless otherwise specified.
    ('Google Arabic Input',
     '{49B24240-CC72-48D7-9A01-6285118C9CA9}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Bengali Input',
     '{446C4D62-5D85-4E6A-845E-FB19AC8C84F8}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Farsi Input',
     '{E0642E36-9D8E-441E-A527-683F77A50FDF}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Greek Input',
     '{45186F45-0E1D-49F3-A534-A52B81F60897}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Gujarati Input',
     '{0693199F-9DF6-4020-B760-CA993177C362}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Hindi Input',
     '{06A2F917-C899-44EE-8F47-5B9128D96B0A}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Japanese Input',  # Not a transliteration IME.
     '{DDCCD2A9-025E-4142-BCEB-F467B88CF830}',
     ' Check http://www.google.com/intl/ja/ime/.'),
    ('Google Kannada Input',
     '{689F4361-5837-4A9C-8BF8-078D04406EC3}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Malayalam Input',
     '{DA2110CA-14F7-4560-A76E-D47345024C49}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Marathi Input',
     '{79D2E710-121A-4892-9541-66740728CEBB}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Nepali Input',
     '{0657DE2E-EC18-4C72-8D58-7D864EA210DE}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Punjabi Input',
     '{A8DE44D0-9B9D-4EAF-BD5B-6411CE79A39E}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Tamil Input',
     '{7498340C-3670-47E3-82AE-1BF2B1D3FCD6}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Telugu Input',
     '{9FF9FAC2-A7E1-4A34-AB91-77AD18CED53F}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ('Google Urdu Input',
     '{311963FF-A0E6-4D8E-BFC7-1C90B261180C}',
     ' Check http://www.google.com/ime/transliteration/.'),
    ]
