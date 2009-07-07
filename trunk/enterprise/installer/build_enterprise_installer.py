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

"""Build an installer for use in enterprise situations.

  This module contains the functionality required to build enterprise
  installers (msi's) for Omaha's various customers.

  The supplied wxs templates need to have an XML extension because SCons
  tries to apply WiX building rules to any input file with the .wxs suffix.

  BuildGoogleUpdateFragment(): Build an update fragment into a .wixobj.
  GenerateNameBasedGUID(): Generate a GUID based on the names supplied.
  BuildEnterpriseInstaller(): Build an msi installer for use in enterprises.
"""

import binascii
import md5

_GOOGLE_UPDATE_NAMESPACE_GUID = 'BE19B3E4502845af8B3E67A99FCDCFB1'

def BuildGoogleUpdateFragment(env,
                              metainstaller_path,
                              product_name,
                              product_version,
                              product_guid,
                              product_custom_params,
                              wixobj_base_name,
                              google_update_wxs_template_path,
                              is_using_google_update_1_2_171_or_later=False):
  """Build an update fragment into a WiX object.

    Takes a supplied wix fragment, and turns it into a .wixobj object for later
    inclusion into an msi.

  Args:
    env: environment to build with
    metainstaller_path: path to the Omaha metainstaller to include
    product_name: name of the product the fragment is being built for
    product_guid: Omaha application ID of the product the fragment is being
        built for
    product_custom_params: custom values to be appended to the Omaha tag
    wixobj_base_name: root of name for the wixobj
    google_update_wxs_template_path: path to the fragment source
    product_version: product version to be installed

  Returns:
    Output object for the built wixobj.

  Raises:
    Nothing.
  """

  product_name_legal_identifier = product_name.replace(' ', '')

  intermediate_base_name = wixobj_base_name + '_google_update_fragment'

  copy_target = env.Command(
      target=intermediate_base_name + '.wxs',
      source=google_update_wxs_template_path,
      action='@copy /y $SOURCE $TARGET',
  )

  wix_defines = [
      '-dProductName="%s"' % product_name,
      '-dProductNameLegalIdentifier="%s"' % product_name_legal_identifier,
      '-dProductVersion=' + product_version,
      '-dProductGuid="%s"' % product_guid,
      '-dProductCustomParams="%s"' % product_custom_params,
      '-dGoogleUpdateMetainstallerPath="%s"' % (
          env.File(metainstaller_path).abspath),
      ]

  if is_using_google_update_1_2_171_or_later:
    wix_defines += [
        '-dUsingGoogleUpdate_1_2_171_OrLater=1'
        ]

  wixobj_output = env.Command(
      target=intermediate_base_name + '.wixobj',
      source=copy_target,
      action='@candle.exe -nologo -out $TARGET $SOURCE ' + ' '.join(wix_defines)
  )

  # Force a rebuild of the .wixobj file when the metainstaller changes.
  # Does not necessarily force rebuild of the MSI because hash does not change.
  env.Depends(wixobj_output, metainstaller_path)

  return wixobj_output


def _BuildMsiForExe(env,
                    enterprise_installer_dir,
                    product_name,
                    product_version,
                    product_installer_path,
                    product_installer_install_command,
                    product_installer_disable_update_registration_arg,
                    product_installer_uninstall_command,
                    msi_base_name,
                    google_update_wixobj_output,
                    metainstaller_path):
  # metainstaller_path: path to the Omaha metainstaller. Should be same file
  #     used for google_update_wixobj_output. Used only to force rebuilds.

  product_name_legal_identifier = product_name.replace(' ', '')
  msi_name = msi_base_name + '.msi'

  omaha_installer_namespace = binascii.a2b_hex(_GOOGLE_UPDATE_NAMESPACE_GUID)

  # Include the .msi filename in the Product Code generation because "the
  # product code must be changed if... the name of the .msi file has been
  # changed" according to http://msdn.microsoft.com/en-us/library/aa367850.aspx.
  msi_product_id = GenerateNameBasedGUID(
      omaha_installer_namespace,
      'Product %s %s' % (product_name, msi_base_name)
  )
  msi_upgradecode_guid = GenerateNameBasedGUID(
      omaha_installer_namespace,
      'Upgrade ' + product_name
  )

  copy_target = env.Command(
      target= msi_base_name + '.wxs',
      source=enterprise_installer_dir + '/enterprise_installer.wxs.xml',
      action='@copy /y $SOURCE $TARGET',
  )

  wix_env = env.Clone()
  wix_env.Append(
      WIXCANDLEFLAGS = [
          '-dProductName=' + product_name,
          '-dProductNameLegalIdentifier=' + product_name_legal_identifier,
          '-dProductVersion=' + product_version,
          '-dProductInstallerPath=' + env.File(product_installer_path).abspath,
          '-dProductInstallerInstallCommand=' +
              product_installer_install_command,
          '-dProductInstallerDisableUpdateRegistrationArg=' +
              product_installer_disable_update_registration_arg,
          '-dProductInstallerUninstallCommand=' +
              product_installer_uninstall_command,
          '-dMsiProductId=' + msi_product_id,
          '-dMsiUpgradeCode=' + msi_upgradecode_guid,
          ],
  )

  wix_output = wix_env.WiX(
      target='unsigned_' + msi_name,
      source=[copy_target, google_update_wixobj_output],
  )

  # Force a rebuild when the installer or metainstaller changes.
  # The metainstaller change does not get passed through even though the .wixobj
  # file is rebuilt because the hash of the .wixobj does not change.
  wix_env.Depends(wix_output,
                  [product_installer_path, metainstaller_path])

  sign_output = wix_env.SignedBinary(
      target=msi_name,
      source=wix_output,
  )

  env.Replicate('$STAGING_DIR', sign_output)


def GenerateNameBasedGUID(namespace, name):
  """Generate a GUID based on the names supplied.

    Follows a methodology recommended in Section 4.3 of RFC 4122 to generate
    a "name-based UUID," which basically means that you want to control the
    inputs to the GUID so that you can generate the same valid GUID each time
    given the same inputs.

    Args:
      namespace: First part of identifier used to generate GUID
      name: Second part of identifier used to generate GUID

    Returns:
      String representation of the generated GUID.

    Raises:
      Nothing.
  """

  # Generate 128 unique bits.
  mymd5 = md5.new()
  mymd5.update(namespace + name)
  hash = mymd5.digest()

  # Set various reserved bits to make this a valid GUID.

  # "Set the four most significant bits (bits 12 through 15) of the
  # time_hi_and_version field to the appropriate 4-bit version number
  # from Section 4.1.3."
  version = ord(hash[6])
  version = 0x30 | (version & 0x0f)

  # "Set the two most significant bits (bits 6 and 7) of the
  # clock_seq_hi_and_reserved to zero and one, respectively."
  clock_seq_hi_and_reserved = ord(hash[8])
  clock_seq_hi_and_reserved = 0x80 | (clock_seq_hi_and_reserved & 0x3f)

  return (
      '%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x' % (
      ord(hash[0]), ord(hash[1]), ord(hash[2]), ord(hash[3]), ord(hash[4]),
      ord(hash[5]), version, ord(hash[7]), clock_seq_hi_and_reserved,
      ord(hash[9]), ord(hash[10]), ord(hash[11]), ord(hash[12]), ord(hash[13]),
      ord(hash[14]), ord(hash[15])))


def BuildEnterpriseInstaller(env,
                             product_name,
                             product_version,
                             product_guid,
                             product_custom_params,
                             product_installer_path,
                             product_installer_install_command,
                             product_installer_disable_update_registration_arg,
                             product_installer_uninstall_command,
                             msi_base_name,
                             enterprise_installer_dir,
                             metainstaller_path,
                             is_using_google_update_1_2_171_or_later=False):
  """Build an installer for use in enterprise situations.

    Builds an msi using the supplied details and binaries. This msi is
    intended to enable enterprise installation scenarios.

  Args:
    env: environment to build with
    product_name: name of the product being built
    product_version: product version to be installed
    product_guid: product's Omaha application ID
    product_custom_params: custom values to be appended to the Omaha tag
    product_installer_path: path to specific product installer
    product_installer_install_command: command line args used to run product
        installer in 'install' mode
    product_installer_disable_update_registration_arg: command line args used
        to run product installer in 'do not register' mode
    product_installer_uninstall_command: command line args used to run product
        installer in 'uninstall' mode
    msi_base_name: root of name for the msi
    enterprise_installer_dir: path to dir which contains
        enterprise_installer.wxs.xml
    metainstaller_path: path to the Omaha metainstaller to include

  Returns:
    Nothing.

  Raises:
    Nothing.
  """

  google_update_wixobj_output = BuildGoogleUpdateFragment(
      env,
      metainstaller_path,
      product_name,
      product_version,
      product_guid,
      product_custom_params,
      msi_base_name,
      enterprise_installer_dir + '/google_update_installer_fragment.wxs.xml',
      is_using_google_update_1_2_171_or_later)

  _BuildMsiForExe(
      env,
      enterprise_installer_dir,
      product_name,
      product_version,
      product_installer_path,
      product_installer_install_command,
      product_installer_disable_update_registration_arg,
      product_installer_uninstall_command,
      msi_base_name,
      google_update_wixobj_output,
      metainstaller_path)

def BuildEnterpriseInstallerFromStandaloneInstaller(
    env,
    product_name,
    product_version,
    product_guid,
    product_custom_params,
    standalone_installer_path,
    product_uninstall_command_line,
    msi_base_name,
    enterprise_installer_dir):
  """Build an installer for use in enterprise situations.

    Builds an msi around the supplied standalone installer. This msi is
    intended to enable enterprise installation scenarios while being as close
    to a normal install as possible. It does not suffer from the separation of
    Omaha and application install like the other methods do.

    This method only works for installers that do not use an MSI.

  Args:
    env: environment to build with
    product_name: name of the product being built
    product_version: product version to be installed
    product_guid: product's Omaha application ID
    product_custom_params: custom values to be appended to the Omaha tag
    standalone_installer_path: path to product's standalone installer
    product_uninstall_command_line: command line used to uninstall the product;
        will be executed directly.
        TODO(omaha): Change this to quiet_uninstall_args and append to uninstall
        string found in registry. Requires a custom action DLL.
    msi_base_name: root of name for the msi
    enterprise_installer_dir: path to dir which contains
        enterprise_standalone_installer.wxs.xml

  Returns:
    Nothing.

  Raises:
    Nothing.
  """
  product_name_legal_identifier = product_name.replace(' ', '')
  msi_name = msi_base_name + '.msi'

  omaha_installer_namespace = binascii.a2b_hex(_GOOGLE_UPDATE_NAMESPACE_GUID)

  # Include the .msi filename in the Product Code generation because "the
  # product code must be changed if... the name of the .msi file has been
  # changed" according to http://msdn.microsoft.com/en-us/library/aa367850.aspx.
  msi_product_id = GenerateNameBasedGUID(
      omaha_installer_namespace,
      'Product %s %s' % (product_name, msi_base_name)
  )
  msi_upgradecode_guid = GenerateNameBasedGUID(
      omaha_installer_namespace,
      'Upgrade ' + product_name
  )

  copy_target = env.Command(
      target= msi_base_name + '.wxs',
      source=(enterprise_installer_dir +
              '/enterprise_standalone_installer.wxs.xml'),
      action='@copy /y $SOURCE $TARGET',
  )

  wix_env = env.Clone()
  wix_env.Append(
      WIXCANDLEFLAGS = [
          '-dProductName=' + product_name,
          '-dProductNameLegalIdentifier=' + product_name_legal_identifier,
          '-dProductVersion=' + product_version,
          '-dProductGuid="%s"' % product_guid,
          '-dProductCustomParams="%s"' % product_custom_params,
          '-dStandaloneInstallerPath=' + (
              env.File(standalone_installer_path).abspath),
          '-dProductUninstallCommandLine=' + product_uninstall_command_line,
          '-dMsiProductId=' + msi_product_id,
          '-dMsiUpgradeCode=' + msi_upgradecode_guid,
          ],
  )

  wix_output = wix_env.WiX(
      target='unsigned_' + msi_name,
      source=[copy_target],
  )

  # Force a rebuild when the standalone installer changes.
  # The metainstaller change does not get passed through even though the .wixobj
  # file is rebuilt because the hash of the .wixobj does not change.
  wix_env.Depends(wix_output, standalone_installer_path)

  sign_output = wix_env.SignedBinary(
      target=msi_name,
      source=wix_output,
  )

  env.Replicate('$STAGING_DIR', sign_output)
