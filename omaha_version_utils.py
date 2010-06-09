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

"""Constants and utilities related to Omaha versions."""

_ONECLICK_ACTIVEX_NAME = 'npGoogleOneClick'
_BHO_NAME = 'GoopdateBho'

# List of languages that are fully supported in the current build.
_OMAHA_LANGUAGES = [
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
    'or',
    'pl',
    'pt-BR',
    'pt-PT',
    'ro',
    'ru',
    'sk',
    'sl',
    'sr',
    'sv',
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
    'am',
    'sw',
    'userdefault',
    'zh-HK',
    ]


# All languages supported by this script currently have the same set of
# languages, so the omaha_version_info parameter is unused.
def _GetMetainstallerPayloadFilenames(prefix,
                                      activex_filename,
                                      bho_filename,
                                      languages,
                                      omaha_version):
  """Returns list of metainstaller payload files for specified Omaha version."""
  if (omaha_version[0] < 1 or
      omaha_version[1] < 2 or
      omaha_version[2] < 183):
    raise Exception('Unsupported version: ' +
                    ConvertVersionToString(omaha_version))

  payload_files = [
      'GoogleUpdate.exe',
      'GoogleCrashHandler.exe',
      '%sgoopdate.dll' % (prefix),
      '%s%s' % (prefix, activex_filename),  # One-Click DLL
      '%s%s' % (prefix, bho_filename),      # BHO proxy DLL
      'GoogleUpdateHelper.msi',
      ]

  for language in languages:
    payload_files += ['%sgoopdateres_%s.dll' % (prefix, language)]

  return payload_files


def ConvertVersionToString(version):
  """Converts a four-element version list to a version string."""
  return '%d.%d.%d.%d' % (version[0], version[1], version[2], version[3])


def GetONECLICK_ACTIVEX_NAME():  # pylint: disable-msg=C6409
  """Returns the value of the ONECLICK_ACTIVEX_NAME define for the C++ code."""
  return _ONECLICK_ACTIVEX_NAME


def GetBHO_NAME():  # pylint: disable-msg=C6409
  """Returns the value of the BHO_NAME define for the C++ code."""
  return _BHO_NAME


def GetLanguagesForVersion(omaha_version):
  """Returns a list of languages supported by omaha_version."""
  if (omaha_version[0] < 1 or
      omaha_version[1] < 2 or
      omaha_version[2] < 183):
    raise Exception('Unsupported version: ' +
                    ConvertVersionToString(omaha_version))

  supported_languages = _OMAHA_LANGUAGES

  # When languages are added, add a version check for older versions without the
  # new languages and remove the new languages from supported_languages.

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
    oneclick_plugin_version: Version of the OneClick plug-in.
    oneclick_plugin_filename: Name of the signed OneClick DLL.
    bho_filename:  Name of the signed BHO DLL.
    oneclick_signed_file_info: SignedFileInfo object for the OneClick DLL.
    bho_signed_file_info: SignedFileInfo object for the BHO DLL.

  """

  def __init__(self, version_file):
    """Initializes the class based on data from a VERSION file."""
    self._ReadFile(version_file)

    self.filename_prefix = ''

    # Objects containing more properties used to build the file.
    self.oneclick_signed_file_info = SignedFileInfo(
        _ONECLICK_ACTIVEX_NAME,
        'dll',
        self.oneclick_plugin_version)
    self.bho_signed_file_info = SignedFileInfo(_BHO_NAME, 'dll')

    # Simple properties for callers that only need the final filename. Not
    # affected by internal build changes.
    self.oneclick_plugin_filename = self.oneclick_signed_file_info.filename
    self.bho_filename = self.bho_signed_file_info.filename

  def _ReadFile(self, version_file):
    """Reads and stores data from a VERSION file."""

    execfile(version_file, globals())

    # Silence Pylint. Values from version_file are not defined in this file.
    # E0602: Undefined variable.
    # pylint: disable-msg=E0602

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

    self.oneclick_plugin_version = oneclick_plugin_version

    # pylint: enable-msg=E0602

  def MakeTestVersion(self, delta=1):
    """Changes this object to be for a TEST version of Omaha."""

    if delta <= 0:
      raise Exception('Delta must be greater than 0.')

    # If we're doing a patch, increment patch; else, increment build.
    if self.version_patch > 0:
      self.version_patch += delta
    else:
      self.version_build += delta

    self.filename_prefix = 'TEST_'

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
                                             self.oneclick_plugin_filename,
                                             self.bho_filename,
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
