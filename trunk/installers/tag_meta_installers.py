#!/usr/bin/python2.4
#
# Copyright 2005-2009 Google Inc.
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

"""Creates application specific meta-installers.

Run this script with the directory that contains the build files,
GoogleupdateSetup_<lang>.exe and pass it the file that contains the application
information to be stamped inside the binary.

"""

import codecs
import os
import sys
import re
import urllib

class Bundle:
  """Represents the information for a bundle"""
  def __init__(self, exe_name, admin, language,
               browser_type, stats, installers_txt_filename, applications):
    self.applications = applications
    self.name = exe_name
    self.needs_admin = admin
    self.lang = language
    self.browser = browser_type
    self.usage_stats = stats
    self.installers_txt_filename = installers_txt_filename
    self.output_file_name = ""

def UrlEncodeString(name):
  """Converts the name into utf8 encoded and then urlencodes it.

  Args:
    name: The string to utf8 convert.

  Returns:
    The utf8 converted urlencoded string.
  """
  utf8_str = name.encode('utf8')
  return urllib.quote(utf8_str)

def ReadBundleInstallerFile(installers_txt_filename):
  """Read the installation file and return a list of the values.
     Only reads information from bundle installer files. The filename
     should contain the "bundle" word.
  Args:
    installers_txt_filename: The file which contains application specific
                             values, of guid, name, exe_name, needs_admin and
                             language.

  Returns:
    A dictionary of Bundles key=language, value=[Bundle]
  """
  bundles = {}
  installers_txt_file = codecs.open(installers_txt_filename, 'r', 'utf16')
  for line in installers_txt_file.readlines():
    line = line.strip()
    if len(line) and not line.startswith('#'):
      (exe_name, needs_admin, language, browser, usage, bundle_apps) =\
        eval(line)
      bundle = Bundle(exe_name, needs_admin, language,
                      browser, usage,
                      installers_txt_filename, bundle_apps)
      if (not bundles.has_key(language)):
        bundles[language] = [bundle]
      else:
        bundles[language] .append(bundle)
  return bundles

def BuildOutputDirectory(output_dir, lang, app):
  """Returns the output directory for the app.

  Args:
    app: The application for which to generate the path.
    lang: The language of the applicaiton.
    output_dir: The output directory.

  Returns:
    The path of the output file.
  """
  installers_txt_filename = app.installers_txt_filename

  end_idx = installers_txt_filename.find('_installers.txt')
  app_dirname = installers_txt_filename[:end_idx]

  app_path = os.path.join(output_dir, app_dirname)
  lang_path = os.path.join(app_path, lang)
  return lang_path


def SetOutputFileNames(file_name, apps, output_dir):
  """Sets the output file names inside the Application.

  Args:
    file_name: The input file name that needs to be stamped. This
               is used for deciding whether to use TEST in the name,
               and the directory. The names of this file is expected
               to be either TEST_GoogleUpdateSetup.exe or
               GoogleUpdateSetup.exe
    apps: Dictionary of applications keyed by language.
    output_dir: The output directory.

  Returns:
    All the applications for a particular language.
  """

  # Determine the language.
  file = os.path.basename(file_name)
  if file.startswith('TEST_'):
    test = True
  else:
    test = False

  for (lang, apps_lang) in apps.iteritems():
    # Get the output filename and set it on the application.
    for app in apps_lang:
      output_path = BuildOutputDirectory(output_dir, lang, app)
      path = os.path.join(output_path, GetOutputFileName(test, app.name, lang))
      app.output_file_name = path

def GetOutputFileName(test, name, lang):
  """Creates the output file name based on the language and the name of the app.
  Args:
    test: Whether the input file name starts with TEST.
    name: The name of the application. I.e. Chrome, GoogleGears.
    lang: The language of the installer.
  Returns:
    The output filename
  """
  if test:
    file_name = 'Tagged_TEST_%sSetup_%s.exe' % (name, lang)
  else:
    file_name = 'Tagged_%sSetup_%s.exe' % (name, lang)
  return file_name

def BuildTagStringForBundle(bundle):
  """Builds the string to be tagged into the binary for a bundle.
  Args:
    bundle: Contains all information about a bundle.
  Returns:
    The string to be tagged into the installer.
  """
  args = '\"'
  first_app = True
  for app in bundle.applications:
    if not first_app:
      args += '&'
    first_app = False
    (guid, name, ap) = app
    display_name = UrlEncodeString(name)
    args += 'appguid=%s&appname=%s&needsadmin=%s' % \
            (guid, display_name, bundle.needs_admin)
    if ap:
      args += '&ap=%s' % ap

  if bundle.usage_stats:
    args += '&usagestats=%s' % bundle.usage_stats
  if bundle.browser:
    args += '&browser=%s' % bundle.browser
  if bundle.lang:
    args += '&lang=%s' % bundle.lang
  args += '\"'
  return args

def TagOneFile(file, app, applytag_exe_name):
  """Tags one file with the information contained inside the application.
  Args:
    file: The input file to be stamped.
    app: Contains all the application data.
    applytag_exe_name: The full path of applytag.exe.
  """
  tag_string = BuildTagStringForBundle(app)

  output_path = app.output_file_name
  if not os.path.exists(file):
    print 'Could not find file %s required for creating %s' % \
          (file, output_path)
    return False

  arguments = [applytag_exe_name,
               file,
               output_path,
               tag_string
              ]
  print 'Building %s with tag %s' % (output_path, tag_string)
  os.spawnv(os.P_WAIT, applytag_exe_name, arguments)
  return True

def GetAllSetupExeInDirectory(dir):
  """Creates a number of application specific binaries for the
     passed in binary.
  Args:
    dir: The input directory.
  Returns:
    A list of files that are match the regex:
    TEST_GoogleUpdateSetup_.*.exe$|^GoogleUpdateSetup_.*.exe$
  """
  ret_files = []
  files = os.listdir(dir)
  for file in files:
    regex = re.compile('^TEST_GoogleUpdateSetup_.*.exe$|'
                       '^GoogleUpdateSetup_.*.exe$')
    if not regex.match(file):
      continue
    ret_files.append(file)
  return ret_files

def TagBinary(apps, file, applytag_exe_name):
  """Creates a number of application specific binaries for the
     passed in binary.
  Args:
    apps: Dictionary of key=lang, value=[Application].
    file: The input file to be stamped.
    applytag_exe_name: The full path to applytag_exe_name.
  """
  if not apps:
    return
  for apps_lang in apps:
    for app in apps[apps_lang]:
      TagOneFile(file, app, applytag_exe_name)

def PrintUsage():
  print ''
  print 'Tool to stamp the Guid, Application name, needs admin into the'
  print 'meta-installer.'
  print 'Reads <installer file> which contains the application name,'
  print 'guid, needs_admin and stamps the meta-installer <file> with'
  print 'these values.'
  print 'For an example of the <installer file> take a look at'
  print '#/installers/googlegears_installer.txt'
  print ''
  print 'tag_meta_installers.py <applytag> <input file> <installer file>\
  <output file>'

def main():
  if len(sys.argv) != 5:
    PrintUsage()
    return

  apply_tag_exe = sys.argv[1]
  input_file = sys.argv[2]
  file_name = sys.argv[3]
  output_dir = sys.argv[4]

  if not os.path.exists(apply_tag_exe):
    print "Could not find applytag.exe"
    PrintUsage()
    return

  if not os.path.exists(file_name):
    print "Invalid filename %s" % file_name
    PrintUsage()
    return

  if not os.path.exists(input_file):
    print "Invalid directory %s" % input_file
    PrintUsage()
    return

  if not os.path.exists(output_dir):
    os.mkdir(output_dir)

  # Read in the installers.txt file.
  apps = ReadBundleInstallerFile(file_name)

  SetOutputFileNames(input_file, apps, output_dir)
  TagBinary(apps, input_file, apply_tag_exe)

if __name__ == '__main__':
  main()
