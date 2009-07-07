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

"""Generates a Group Policy template file for Google Update policies.

This script only works on Windows because gpedit.msc requires Windows-style
line endings (\r\n).

To unit test this module, just run the file from the command line.
"""

import filecmp
import os
import sys


HORIZONTAL_RULE = ';%s\n' % ('-' * 78)
MAIN_POLICY_KEY = "Software\Policies\Google\Update"

# pylint: disable-msg=C6004
HEADER = """\
CLASS MACHINE
  CATEGORY !!Cat_Google
    CATEGORY !!Cat_GoogleUpdate
      KEYNAME \"""" + MAIN_POLICY_KEY + """\"
      EXPLAIN !!Explain_GoogleUpdate
"""

PREFERENCES = """
      CATEGORY !!Cat_Preferences
        KEYNAME \"""" + MAIN_POLICY_KEY + """\"
        EXPLAIN !!Explain_Preferences

        POLICY !!Pol_AutoUpdateCheckPeriod
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_2_145_5
          #endif
          EXPLAIN !!Explain_AutoUpdateCheckPeriod
          PART !!Part_AutoUpdateCheckPeriod NUMERIC
            VALUENAME AutoUpdateCheckPeriodMinutes
            DEFAULT 1400  ; 23 hours 20 minutes.
            MIN 60
            MAX 43200     ; 30 days.
            SPIN 60       ; Increment in hour chunks.
          END PART
          PART !!Part_DisableAllAutoUpdateChecks CHECKBOX
            VALUENAME DisableAutoUpdateChecksCheckboxValue  ; Required, unused.
            ACTIONLISTON
              ; Writes over Part_AutoUpdateCheckPeriod. Assumes this runs last.
              VALUENAME AutoUpdateCheckPeriodMinutes VALUE NUMERIC 0
            END ACTIONLISTON
            ACTIONLISTOFF
              ; Do nothing. Let Part_AutoUpdateCheckPeriod take effect.
            END ACTIONLISTOFF
            VALUEOFF  NUMERIC 0
            VALUEON   NUMERIC 1
          END PART
        END POLICY

      END CATEGORY  ; Preferences
"""

APPLICATIONS_HEADER = """
      CATEGORY !!Cat_Applications
        KEYNAME \"""" + MAIN_POLICY_KEY + """\"
        EXPLAIN !!Explain_Applications
"""

UPDATE_POLICY_ITEMLIST = """\
            ITEMLIST
              NAME  !!Name_AutomaticUpdates
              VALUE NUMERIC 1
              NAME  !!Name_ManualUpdates
              VALUE NUMERIC 2
              NAME  !!Name_UpdatesDisabled
              VALUE NUMERIC 0
            END ITEMLIST
            REQUIRED"""

APPLICATION_DEFAULTS = ("""
        POLICY !!Pol_DefaultAllowInstallation
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_2_145_5
          #endif
          EXPLAIN !!Explain_DefaultAllowInstallation
          VALUENAME InstallDefault
          VALUEOFF  NUMERIC 0
          VALUEON   NUMERIC 1
        END POLICY

        POLICY !!Pol_DefaultUpdatePolicy
          #if version >= 4
            SUPPORTED !!Sup_GoogleUpdate1_2_145_5
          #endif
          EXPLAIN !!Explain_DefaultUpdatePolicy
          PART !!Part_UpdatePolicy DROPDOWNLIST
            VALUENAME UpdateDefault
""" +
UPDATE_POLICY_ITEMLIST + """
          END PART
        END POLICY
""")

APP_POLICIES_TEMPLATE = ("""
        CATEGORY !!Cat_$AppLegalId$
          KEYNAME \"""" + MAIN_POLICY_KEY + """\"

          POLICY !!Pol_AllowInstallation
            #if version >= 4
              SUPPORTED !!Sup_GoogleUpdate1_2_145_5
            #endif
            EXPLAIN !!Explain_Install$AppLegalId$
            VALUENAME Install$AppGuid$
            VALUEOFF  NUMERIC 0
            VALUEON   NUMERIC 1
          END POLICY

          POLICY !!Pol_UpdatePolicy
            #if version >= 4
              SUPPORTED !!Sup_GoogleUpdate1_2_145_5
            #endif
            EXPLAIN !!Explain_AutoUpdate$AppLegalId$
            PART !!Part_UpdatePolicy DROPDOWNLIST
              VALUENAME Update$AppGuid$
""" +
UPDATE_POLICY_ITEMLIST.replace('            ', '              ') + """
            END PART
          END POLICY

        END CATEGORY  ; $AppName$
""")

APPLICATIONS_FOOTER = """
      END CATEGORY  ; Applications

    END CATEGORY  ; GoogleUpdate

  END CATEGORY  ; Google
"""

# Policy names that are used in multiple locations.
ALLOW_INSTALLATION_POLICY = 'Allow installation'
DEFAULT_ALLOW_INSTALLATION_POLICY = ALLOW_INSTALLATION_POLICY + ' default'
UPDATE_POLICY = 'Update policy override'
DEFAULT_UPDATE_POLICY = UPDATE_POLICY + ' default'

# Update policy options that are used in multiple locations.
AUTOMATIC_UPDATES = 'Automatic silent updates'
MANUAL_UPDATES = 'Manual updates only'
UPDATES_DISABLED = 'Updates disabled'

# Category names that are used in multiple locations.
PREFERENCES_CATEGORY = 'Preferences'
APPLICATIONS_CATEGORY = 'Applications'

# The captions for update policy were selected such that they appear in order of
# decreasing preference when organized alphabetically in gpedit.
STRINGS_HEADER_AND_COMMON = ('\n' +
HORIZONTAL_RULE +
"""
[strings]
Sup_GoogleUpdate1_2_145_5=At least Google Update 1.2.145.5

Cat_Google=Google
Cat_GoogleUpdate=Google Update
Cat_Preferences=""" + PREFERENCES_CATEGORY + """
Cat_Applications=""" + APPLICATIONS_CATEGORY + """

Pol_AutoUpdateCheckPeriod=Auto-update check period override
Pol_DefaultAllowInstallation=""" + DEFAULT_ALLOW_INSTALLATION_POLICY + """
Pol_AllowInstallation=""" + ALLOW_INSTALLATION_POLICY + """
Pol_DefaultUpdatePolicy=""" + DEFAULT_UPDATE_POLICY + """
Pol_UpdatePolicy=""" + UPDATE_POLICY + """

Part_AutoUpdateCheckPeriod=Minutes between update checks
Part_DisableAllAutoUpdateChecks=Disable all auto-update checks (not recommended)
Part_UpdatePolicy=Policy

Name_AutomaticUpdates=""" + AUTOMATIC_UPDATES + """ (recommended)
Name_ManualUpdates=""" + MANUAL_UPDATES + """
Name_UpdatesDisabled=""" + UPDATES_DISABLED + """

""")

STRINGS_APP_NAME_TEMPLATE = """\
Cat_$AppLegalId$=$AppName$
"""

# pylint: disable-msg=C6310
# pylint: disable-msg=C6013

# "application's" should be preceeded by a different word in different contexts.
# The word is specified by replacing the $PreApplicationWord$ token.
STRINGS_UPDATE_POLICY_OPTIONS = """\
    \\n\\nOptions:\\
    \\n - """ + AUTOMATIC_UPDATES + """: Updates are automatically applied when they are found via the periodic update check.\\
    \\n - """ + MANUAL_UPDATES + """: Updates are only applied when the user does a manual update check. (Not all apps provide an interface for this.)\\
    \\n - """ + UPDATES_DISABLED + """: Never apply updates.\\
    \\n\\nIf you select manual updates, you should periodically check for updates using $PreApplicationWord$ application's manual update mechanism if available. If you disable updates, you should periodically check for updates and distribute them to users."""

STRINGS_COMMON_EXPLANATIONS = ("""
Explain_GoogleUpdate=Policies to control the installation and updating of Google applications that use Google Update/Google Installer.

""" +
HORIZONTAL_RULE +
'; ' + PREFERENCES_CATEGORY + '\n' +
HORIZONTAL_RULE + """
Explain_Preferences=General policies for Google Update.

Explain_AutoUpdateCheckPeriod=Minimum number of minutes between automatic update checks.

""" +
HORIZONTAL_RULE +
'; ' + APPLICATIONS_CATEGORY + '\n' +
HORIZONTAL_RULE + """
Explain_Applications=Policies for individual applications.\\
    \\n\\nAn updated ADM template will be required to support Google applications released in the future.

Explain_DefaultAllowInstallation=Specifies the default behavior for whether Google software can be installed using Google Update/Google Installer.\\
    \\n\\nCan be overridden by the \"""" + ALLOW_INSTALLATION_POLICY + """\" for individual applications.\\
    \\n\\nOnly affects installation of Google software using Google Update/Google Installer. Cannot prevent running the application installer directly or installation of Google software that does not use Google Update/Google Installer for installation.

Explain_DefaultUpdatePolicy=Specifies the default policy for software updates from Google.\\
    \\n\\nCan be overridden by the \"""" + UPDATE_POLICY + """\" for individual applications.\\
""" +
STRINGS_UPDATE_POLICY_OPTIONS.replace('$PreApplicationWord$', 'each') + """\\
    \\n\\nOnly affects updates for Google software that uses Google Update for updates. Does not prevent auto-updates of Google software that does not use Google Update for updates.\\
    \\n\\nUpdates for Google Update are not affected by this setting; Google Update will continue to update itself while it is installed.\\
    \\n\\nWARNING: Disabing updates will also prevent updates of any new Google applications released in the future, possibly including dependencies for future versions of installed applications.

""" +
HORIZONTAL_RULE +
'; Individual Applications\n' +
HORIZONTAL_RULE)

STRINGS_APP_POLICY_EXPLANATIONS_TEMPLATE = ("""
; $AppName$
Explain_Install$AppLegalId$=Specifies whether $AppName$ can be installed using Google Update/Google Installer.\\
    \\n\\nIf this policy is not configured, $AppName$ can be installed as specified by \"""" + DEFAULT_ALLOW_INSTALLATION_POLICY + """\".

Explain_AutoUpdate$AppLegalId$=Specifies how Google Update handles available $AppName$ updates from Google.\\
    \\n\\nIf this policy is not configured, Google Update handles available updates as specified by \"""" + DEFAULT_UPDATE_POLICY + """\".\\
""" +
STRINGS_UPDATE_POLICY_OPTIONS.replace('$PreApplicationWord$', 'the') + '$AppUpdateExplainExtra$\n')

# pylint: enable-msg=C6013
# pylint: enable-msg=C6310
# pylint: enable-msg=C6004


def GenerateGroupPolicyTemplate(apps):
  """Generates a Group Policy template (ADM format)for the specified apps.

  Args:
    apps: A list of tuples containing information about each app.
        Each element of the list is a tuple of:
          * app name
          * app ID
          * optional string to append to the auto-update explanation
            - Should start with a space or double new line (\n\n).
    target_path: Output path of the .ADM template file.

  Returns:
    String containing the contents of the .ADM file.
  """

  def _CreateLegalIdentifier(input_string):
    """Converts input_string to a legal identifier for ADM files.

    Changes some characters that do not necessarily cause problems and may not
    handle all cases.

    Args:
      input_string: Text to convert to a legal identifier.

    Returns:
      String containing a legal identifier based on input_string.
    """

    # pylint: disable-msg=C6004
    return (input_string.replace(' ', '')
                        .replace('&', '')
                        .replace('=', '')
                        .replace(';', '')
                        .replace(',', '')
                        .replace('.', '')
                        .replace('?', '')
                        .replace('=', '')
                        .replace(';', '')
                        .replace("'", '')
                        .replace('"', '')
                        .replace('\\', '')
                        .replace('/', '')
                        .replace('(', '')
                        .replace(')', '')
                        .replace('[', '')
                        .replace(']', '')
                        .replace('{', '')
                        .replace('}', '')
                        .replace('-', '')
                        .replace('!', '')
                        .replace('@', '')
                        .replace('#', '')
                        .replace('$', '')
                        .replace('%', '')
                        .replace('^', '')
                        .replace('*', '')
                        .replace('+', ''))
    # pylint: enable-msg=C6004

  def _WriteTemplateForApp(template, app):
    """Writes the text for the specified app based on the template.

    Replaces $AppName$, $AppLegalId$, $AppGuid$, and $AppUpdateExplainExtra$.

    Args:
      template: text to process and write.
      app: tuple containing information about the app.

    Returns:
      String containing a copy of the template populated with app-specific
      strings.
    """

    (app_name, app_guid, update_explain_extra) = app
    # pylint: disable-msg=C6004
    return (template.replace('$AppName$', app_name)
                    .replace('$AppLegalId$', _CreateLegalIdentifier(app_name))
                    .replace('$AppGuid$', app_guid)
                    .replace('$AppUpdateExplainExtra$', update_explain_extra)
           )
    # pylint: enable-msg=C6004

  def _WriteTemplateForAllApps(template, apps):
    """Writes a copy of the template for each of the specified apps.

    Args:
      template: text to process and write.
      apps: list of tuples containing information about the apps.

    Returns:
      String containing concatenated copies of the template for each app in
      apps, each populated with the appropriate app-specific strings.
    """

    content = [_WriteTemplateForApp(template, app) for app in apps]
    return ''.join(content)

  target_contents = [
      HEADER,
      PREFERENCES,
      APPLICATIONS_HEADER,
      APPLICATION_DEFAULTS,
      _WriteTemplateForAllApps(APP_POLICIES_TEMPLATE, apps),
      APPLICATIONS_FOOTER,
      STRINGS_HEADER_AND_COMMON,
      _WriteTemplateForAllApps(STRINGS_APP_NAME_TEMPLATE, apps),
      STRINGS_COMMON_EXPLANATIONS,
      _WriteTemplateForAllApps(STRINGS_APP_POLICY_EXPLANATIONS_TEMPLATE, apps),
      ]

  return ''.join(target_contents)


# Run a unit test when the module is run directly.
if __name__ == '__main__':
  TEST_APPS = [
      ('Google Chrome',
       '{8A69D345-D564-463C-AFF1-A69D9E530F96}',
       ' Check http://www.google.com/chrome/.'),
      ('Google Earth',
       '{74AF07D8-FB8F-4D51-8AC7-927721D56EBB}',
       ' Check http://earth.google.com/.'),
      ]
  TEST_GOLD_FILENAME = 'test_gold.adm'
  TEST_OUTPUT_FILENAME = 'test_out.adm'

  module_dir = os.path.abspath(os.path.dirname(__file__))
  gold_path = os.path.join(module_dir, TEST_GOLD_FILENAME)
  output_path = os.path.join(module_dir, TEST_OUTPUT_FILENAME)

  test_target_contents = GenerateGroupPolicyTemplate(TEST_APPS)

  target = open(output_path, 'wt')
  target.write(test_target_contents)
  target.close()

  if filecmp.cmp(gold_path, output_path, shallow=False):
    print 'PASS: Contents equal.'
  else:
    print 'FAIL: Contents not equal.'
    sys.exit(-1)
