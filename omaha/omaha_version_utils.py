#!/usr/bin/python2.4
#
# Copyright 2010 Google Inc.
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

"""Constants and utilities related to Omaha and tools versions"""

_ONECLICK_PLUGIN_NAME = 'npGoogleOneClick'
_UPDATE_PLUGIN_NAME = 'npGoogleUpdate'
_MAIN_EXE_BASE_NAME = 'GoogleUpdate'
_CRASH_HANDLER_NAME = 'GoogleCrashHandler'

# List of languages that are fully supported in the current build.
_OMAHA_LANGUAGES = [
    'am',
    'ar',
    'bg',
    'bn',
    'ca',
    'cs',
    'da',
    'de',
    'el',
    'en',
    'en-GB',
    'es',
    'es-419',
    'et',
    'fa',
    'fi',
    'fil',
    'fr',
    'gu',
    'hi',
    'hr',
    'hu',
    'id',
    'is',
    'it',
    'iw',
    'ja',
    'kn',
    'ko',
    'lt',
    'lv',
    'ml',
    'mr',
    'ms',
    'nl',
    'no',
    'pl',
    'pt-BR',
    'pt-PT',
    'ro',
    'ru',
    'sk',
    'sl',
    'sr',
    'sv',
    'sw',
    'ta',
    'te',
    'th',
    'tr',
    'uk',
    'ur',
    'vi',
    'zh-CN',
    'zh-TW',
    ]

# The shell and goopdate.dll contain additional languages.
# 'userdefault' addresses apps that don't look up the resource for the OS
# language. See http://b/1328652.
_ADDITIONAL_SHELL_LANGUAGES = [
    'or',
    'userdefault',
    'zh-HK',
    ]

VC71  = 1310  # VC2003/VC71 (not supported by the current build).
VC80  = 1400  # VC2005/VC80
VC90  = 1500  # VC2008/VC90 (not supported by the current build).
VC100 = 1600  # VC2010/VC10
VC110 = 1700  # VC2012/VC11 (not supported by the current build).
VC120 = 1800  # VC2013/VC12
VC140 = 1900  # VC2015/VC14
VC150 = 1910  # VC2017 version 15.0-15.9 / VC14.1-14.16
VC160 = 1920  # VC2019 version 16.0 / VC14.20
VC170 = 1930  # VC2019 version 17.0 / VC14.30


def _IsSupportedOmaha2Version(omaha_version):
  """Returns true if omaha_version is an Omaha 2 version and is supported."""
  return (omaha_version[0] == 1 and
          omaha_version[1] == 2 and
          omaha_version[2] >= 183)


# All languages supported by this script currently have the same set of
# languages, so the omaha_version_info parameter is unused.
def _GetMetainstallerPayloadFilenames(prefix,
                                      languages,
                                      omaha_version):
  """Returns list of metainstaller payload files for specified Omaha version."""

  # The list of files below needs to be kept in sync with the list in
  # SetupFiles::BuildFileLists().
  # TODO(omaha): Move the other filename defines in main.scons into this file
  # and allow all filenames to be customized.
  payload_files = [
      '%s.exe' % _MAIN_EXE_BASE_NAME,
      '%s.exe' % _CRASH_HANDLER_NAME,
      '%sgoopdate.dll' % (prefix),
      '%sHelper.msi' % _MAIN_EXE_BASE_NAME,
      '%sBroker.exe' % _MAIN_EXE_BASE_NAME,
      '%sOnDemand.exe' % _MAIN_EXE_BASE_NAME,
      '%sComRegisterShell64.exe' % _MAIN_EXE_BASE_NAME,
      '%spsmachine.dll' % (prefix),
      '%spsmachine_64.dll' % (prefix),
      '%spsuser.dll' % (prefix),
      '%spsuser_64.dll' % (prefix),
      ]

  if _IsSupportedOmaha2Version(omaha_version):
    payload_files.remove('%sBroker.exe' % _MAIN_EXE_BASE_NAME)
    payload_files.remove('%sOnDemand.exe' % _MAIN_EXE_BASE_NAME)
    payload_files.remove('%sComRegisterShell64.exe' % _MAIN_EXE_BASE_NAME)
    payload_files.remove('psmachine.dll')
    payload_files.remove('psmachine_64.dll')
    payload_files.remove('psuser.dll')
    payload_files.remove('psuser_64.dll')
  elif (omaha_version[0] <= 1 and
        omaha_version[1] <= 3 and
        omaha_version[2] < 13):
    raise Exception('Unsupported version: ' +
                    ConvertVersionToString(omaha_version))

  if (omaha_version[0] >= 1 and
      omaha_version[1] >= 3 and
      (omaha_version[2] >= 22 or
       (omaha_version[2] == 21 and omaha_version[3] >= 85))):
    # 64-bit crash handler is added on 1.3.21.85 and later
    payload_files.append('%s64.exe' % _CRASH_HANDLER_NAME)

  if (omaha_version[0] >= 1 and
      omaha_version[1] >= 3 and
      (omaha_version[2] >= 32)):
    # added with 1.3.32.1 and later
    payload_files.append('%sCore.exe' % _MAIN_EXE_BASE_NAME)

  if (omaha_version[0] >= 1 and
      omaha_version[1] >= 3 and
      (omaha_version[2] > 36 or
       (omaha_version[2] == 36 and omaha_version[3] >= 61))):
    # GoogleUpdateHelper.msi was removed with version 1.3.36.61.
    payload_files.remove('%sHelper.msi' % _MAIN_EXE_BASE_NAME)

  for language in languages:
    payload_files += ['%sgoopdateres_%s.dll' % (prefix, language)]

  return payload_files


def ConvertVersionToString(version):
  """Converts a four-element version list to a version string."""
  return '%d.%d.%d.%d' % (version[0], version[1], version[2], version[3])


def GetONECLICK_PLUGIN_NAME():  # pylint: disable-msg=C6409
  """Returns the value of the ONECLICK_PLUGIN_NAME define for the C++ code."""
  return _ONECLICK_PLUGIN_NAME


def GetUPDATE_PLUGIN_NAME():  # pylint: disable-msg=C6409
  """Returns the value of the UPDATE_PLUGIN_NAME define for the C++ code."""
  return _UPDATE_PLUGIN_NAME


def GetCRASH_HANDLER_NAME():  # pylint: disable-msg=C6409
  """Returns the value of the CRASH_HANDLER_NAME define for the C++ code."""
  return _CRASH_HANDLER_NAME


def GetLanguagesForVersion(omaha_version):
  """Returns a list of languages supported by omaha_version."""
  # Make a copy in case the list is modified below.
  supported_languages = list(_OMAHA_LANGUAGES)

  # When languages are added, add a version check for older versions without the
  # new languages and remove the new languages from supported_languages.

  if (omaha_version[0] == 1 and
      omaha_version[1] == 3 and
      omaha_version[2] >= 21):
    # All languages are supported.
    pass
  elif _IsSupportedOmaha2Version(omaha_version):
    # All current languages are supported. 'or' was also supported.
    supported_languages += ['or']
    supported_languages.remove('am')
    supported_languages.remove('sw')
  else:
    raise Exception('Unsupported version: ' +
                    ConvertVersionToString(omaha_version))

  return supported_languages


def GetShellLanguagesForVersion(omaha_version):
  """Returns a list of languages supported by the omaha_version shell."""

  # Silence PyLint. All languages supported by this script currently have the
  # same set of languages, so this variable is unused.
  omaha_version = omaha_version

  return _OMAHA_LANGUAGES + _ADDITIONAL_SHELL_LANGUAGES


class OmahaVersionInfo(object):
  """Contains information about a specific version of Omaha.

  Attributes:
    filename_prefix: Prefix to use for all output files.
    version_major: Major version.
    version_minor: Minor version.
    version_build: Build version.
    version_patch: Patch version.
    crash_handler_filename: Name of the Crash Handler EXE.
  """

  def __init__(self, version_file):
    """Initializes the class based on data from a VERSION file."""
    self._ReadFile(version_file)

    self.filename_prefix = ''

  def _ReadFile(self, version_file):
    """Reads and stores data from a VERSION file."""

    execfile(version_file, globals())

    if version_patch > 0:
      incrementing_value = version_patch
      incrementing_value_name = 'patch'
    else:
      incrementing_value = version_build
      incrementing_value_name = 'build'
    if 0 == incrementing_value % 2:
      raise Exception('ERROR: By convention, the %s number in VERSION '
                      '(currently %d) should be odd.' %
                      (incrementing_value_name, incrementing_value))

    self.version_major = version_major
    self.version_minor = version_minor
    self.version_build = version_build
    self.version_patch = version_patch

  def MakeTestVersion(self, delta, prefix):
    """Changes this object to be for a TEST version of Omaha."""

    if delta <= 0:
      raise Exception('Invalid version delta.')

    if prefix and prefix != 'TEST_' and prefix != 'TEST2_':
      raise Exception('Unrecognized prefix "%s"' % prefix)

    # If we're doing a patch, increment patch; else, increment build.
    if self.version_patch > 0:
      self.version_patch += delta
    else:
      self.version_build += delta

    self.filename_prefix = prefix

  def GetVersion(self):
    """Returns the version elements as a list."""
    return [self.version_major,
            self.version_minor,
            self.version_build,
            self.version_patch
           ]

  def GetVersionString(self):
    """Returns the version as a string."""
    return ConvertVersionToString(self.GetVersion())

  def GetSupportedLanguages(self):
    """Returns a list of languages supported by this version."""
    return GetLanguagesForVersion(self.GetVersion())

  def GetMetainstallerPayloadFilenames(self):
    """Returns list of metainstaller payload files for this version of Omaha."""
    return _GetMetainstallerPayloadFilenames(self.filename_prefix,
                                             self.GetSupportedLanguages(),
                                             self.GetVersion())


class SignedFileInfo(object):
  """Contains information, including intermediate names, for signed file."""

  def __init__(self, unversioned_name, extension, file_version=None):
    """Initializes the class members based on the parameters."""

    if file_version:
      base_name = '%s%d' % (unversioned_name, file_version)
    else:
      base_name = unversioned_name

    self.filename_base = base_name
    self.filename = '%s.%s' % (self.filename_base, extension)

    self.unsigned_filename_base = '%s_unsigned' % base_name
    self.unsigned_filename = '%s.%s' % (self.unsigned_filename_base, extension)
