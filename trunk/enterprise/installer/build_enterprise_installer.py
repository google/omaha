#!/usr/bin/python2.4
#
# Copyright 2009-2010 Google Inc.
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
  installers (MSIs) for Omaha's various customers.

  The supplied wxs templates need to have an XML extension because SCons
  tries to apply WiX building rules to any input file with the .wxs suffix.

  BuildGoogleUpdateFragment(): Build an update fragment into a .wixobj.
  GenerateNameBasedGUID(): Generate a GUID based on the names supplied.
  BuildEnterpriseInstaller(): Build an MSI installer for use in enterprises.
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
                              google_update_wxs_template_path):
  """Build an update fragment into a WiX object.

    Takes a supplied wix fragment, and turns it into a .wixobj object for later
    inclusion into an MSI.

  Args:
    env: environment to build with
    metainstaller_path: path to the Omaha metainstaller to include
    product_name: name of the product the fragment is being built for
    product_version: product version to be installed
    product_guid: Omaha application ID of the product the fragment is being
        built for
    product_custom_params: custom values to be appended to the Omaha tag
    wixobj_base_name: root of name for the wixobj
    google_update_wxs_template_path: path to the fragment source

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
                    product_name,
                    product_version,
                    product_guid,
                    product_installer_path,
                    product_installer_install_command,
                    product_installer_disable_update_registration_arg,
                    product_uninstaller_additional_args,
                    msi_base_name,
                    google_update_wixobj_output,
                    enterprise_installer_dir,
                    metainstaller_path,
                    output_dir):
  """Build an MSI installer for use in enterprise situations.

    Builds an MSI for the executable installer at product_installer_path using
    the supplied details. Requires an existing Google Update installer fragment
    as well as a path to a custom action DLL containing the logic to launch the
    product's uninstaller.

    This is intended to enable enterprise installation scenarios.

  Args:
    env: environment to build with
    product_name: name of the product being built
    product_version: product version to be installed
    product_guid: product's Omaha application ID
    product_installer_path: path to specific product installer
    product_installer_install_command: command line args used to run product
        installer in 'install' mode
    product_installer_disable_update_registration_arg: command line args used
        to run product installer in 'do not register' mode
    product_uninstaller_additional_args: extra command line parameters that the
        custom action dll will pass on to the product uninstaller, typically
        you'll want to pass any extra arguments that will force the uninstaller
        to run silently here.
    msi_base_name: root of name for the MSI
    google_update_wixobj_output: the MSI fragment containing the Omaha
        installer.
    enterprise_installer_dir: path to dir which contains
        enterprise_installer.wxs.xml
    metainstaller_path: path to the Omaha metainstaller. Should be same file
        used for google_update_wixobj_output. Used only to force rebuilds.
    output_dir: path to the directory that will contain the resulting MSI

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
      target=msi_base_name + '.wxs',
      source=enterprise_installer_dir + '/enterprise_installer.wxs.xml',
      action='@copy /y $SOURCE $TARGET',
  )

  wix_env = env.Clone()
  wix_env.Append(
      WIXCANDLEFLAGS=[
          '-dProductName=' + product_name,
          '-dProductNameLegalIdentifier=' + product_name_legal_identifier,
          '-dProductVersion=' + product_version,
          '-dProductGuid=' + product_guid,
          '-dProductInstallerPath=' + env.File(product_installer_path).abspath,
          '-dProductInstallerInstallCommand=' + (
              product_installer_install_command),
          '-dProductInstallerDisableUpdateRegistrationArg=' + (
              product_installer_disable_update_registration_arg),
          '-dProductUninstallerAdditionalArgs=' + (
              product_uninstaller_additional_args),
          '-dMsiProductId=' + msi_product_id,
          '-dMsiUpgradeCode=' + msi_upgradecode_guid,
          ],
  )

  # Disable warning LGHT1076 which complains about a string-length ICE error
  # that can safely be ignored as per
  # http://blogs.msdn.com/astebner/archive/2007/02/13/building-an-msi-
  # using-wix-v3-0-that-includes-the-vc-8-0-runtime-merge-modules.aspx
  wix_env['WIXLIGHTFLAGS'].append('-sw1076')

  wix_output = wix_env.WiX(
      target='unsigned_' + msi_name,
      source=[copy_target, google_update_wixobj_output],
  )

  # Force a rebuild when the installer or metainstaller changes.
  # The metainstaller change does not get passed through even though the .wixobj
  # file is rebuilt because the hash of the .wixobj does not change.
  # Also force a dependency on the CA DLL. Otherwise, it might not be built
  # before the MSI.
  wix_env.Depends(wix_output, [product_installer_path, metainstaller_path])

  sign_output = wix_env.SignedBinary(
      target=msi_name,
      source=wix_output,
  )

  env.Replicate(output_dir, sign_output)


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
  md5_hash = mymd5.digest()

  # Set various reserved bits to make this a valid GUID.

  # "Set the four most significant bits (bits 12 through 15) of the
  # time_hi_and_version field to the appropriate 4-bit version number
  # from Section 4.1.3."
  version = ord(md5_hash[6])
  version = 0x30 | (version & 0x0f)

  # "Set the two most significant bits (bits 6 and 7) of the
  # clock_seq_hi_and_reserved to zero and one, respectively."
  clock_seq_hi_and_reserved = ord(md5_hash[8])
  clock_seq_hi_and_reserved = 0x80 | (clock_seq_hi_and_reserved & 0x3f)

  return (
      '%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x' % (
          ord(md5_hash[0]), ord(md5_hash[1]), ord(md5_hash[2]),
          ord(md5_hash[3]),
          ord(md5_hash[4]), ord(md5_hash[5]),
          version, ord(md5_hash[7]),
          clock_seq_hi_and_reserved, ord(md5_hash[9]),
          ord(md5_hash[10]), ord(md5_hash[11]), ord(md5_hash[12]),
          ord(md5_hash[13]), ord(md5_hash[14]), ord(md5_hash[15])))


def BuildEnterpriseInstaller(env,
                             product_name,
                             product_version,
                             product_guid,
                             product_custom_params,
                             product_installer_path,
                             product_installer_install_command,
                             product_installer_disable_update_registration_arg,
                             product_uninstaller_additional_args,
                             msi_base_name,
                             enterprise_installer_dir,
                             metainstaller_path,
                             output_dir='$STAGING_DIR'):
  """Build an installer for use in enterprise situations.

    Builds an MSI using the supplied details and binaries. This MSI is
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
    product_uninstaller_additional_args: extra command line parameters that the
        custom action dll will pass on to the product uninstaller, typically
        you'll want to pass any extra arguments that will force the uninstaller
        to run silently here.
    msi_base_name: root of name for the MSI
    enterprise_installer_dir: path to dir which contains
        enterprise_installer.wxs.xml
    metainstaller_path: path to the Omaha metainstaller to include
    output_dir: path to the directory that will contain the resulting MSI

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
      enterprise_installer_dir + '/google_update_installer_fragment.wxs.xml')

  _BuildMsiForExe(
      env,
      product_name,
      product_version,
      product_guid,
      product_installer_path,
      product_installer_install_command,
      product_installer_disable_update_registration_arg,
      product_uninstaller_additional_args,
      msi_base_name,
      google_update_wixobj_output,
      enterprise_installer_dir,
      metainstaller_path,
      output_dir)


def BuildEnterpriseInstallerFromStandaloneInstaller(
    env,
    product_name,
    product_version,
    product_guid,
    product_custom_params,
    product_uninstaller_additional_args,
    product_installer_data,
    standalone_installer_path,
    msi_base_name,
    enterprise_installer_dir,
    output_dir='$STAGING_DIR'):
  """Build an installer for use in enterprise situations.

    Builds an MSI around the supplied standalone installer. This MSI is
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
    product_uninstaller_additional_args: extra command line parameters that the
        custom action dll will pass on to the product uninstaller, typically
        you'll want to pass any extra arguments that will force the uninstaller
        to run silently here.
    product_installer_data: installer data to be passed to the
        product installer at run time. This is useful as an alternative to
        the product_installer_install_command parameter accepted by
        BuildEnterpriseInstaller() since command line parameters can't be
        passed to the product installer when it is wrapped in a standalone
        installer.
    standalone_installer_path: path to product's standalone installer
    msi_base_name: root of name for the MSI
    enterprise_installer_dir: path to dir which contains
        enterprise_standalone_installer.wxs.xml
    output_dir: path to the directory that will contain the resulting MSI

  Returns:
    Target nodes.

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

  # To allow for multiple versions of the same product to be generated,
  # stick output in a subdirectory.
  output_directory_name = product_guid + '.' + product_version

  copy_target = env.Command(
      target=output_directory_name + msi_base_name + '.wxs',
      source=(enterprise_installer_dir +
              '/enterprise_standalone_installer.wxs.xml'),
      action='@copy /y $SOURCE $TARGET',
  )

  wix_env = env.Clone()
  wix_candle_flags = [
      '-dProductName=' + product_name,
      '-dProductNameLegalIdentifier=' + product_name_legal_identifier,
      '-dProductVersion=' + product_version,
      '-dProductGuid="%s"' % product_guid,
      '-dProductCustomParams="%s"' % product_custom_params,
      '-dStandaloneInstallerPath=' + (
          env.File(standalone_installer_path).abspath),
      '-dProductUninstallerAdditionalArgs=' + (
          product_uninstaller_additional_args),
      '-dMsiProductId=' + msi_product_id,
      '-dMsiUpgradeCode=' + msi_upgradecode_guid,
  ]

  if product_installer_data:
    wix_candle_flags.append('-dProductInstallerData=' + product_installer_data)

  wix_env.Append(
      WIXCANDLEFLAGS=wix_candle_flags
  )

  # Disable warning LGHT1076 which complains about a string-length ICE error
  # that can safely be ignored as per
  # http://blogs.msdn.com/astebner/archive/2007/02/13/building-an-msi-
  # using-wix-v3-0-that-includes-the-vc-8-0-runtime-merge-modules.aspx
  wix_env['WIXLIGHTFLAGS'].append('-sw1076')

  wix_output = wix_env.WiX(
      target = output_directory_name + '/' + 'unsigned_' + msi_name,
      source = [copy_target],
  )

  # Force a rebuild when the standalone installer changes.
  # The metainstaller change does not get passed through even though the .wixobj
  # file is rebuilt because the hash of the .wixobj does not change.
  # Also force a dependency on the CA DLL. Otherwise, it might not be built
  # before the MSI.
  wix_env.Depends(wix_output, [standalone_installer_path])

  sign_output = wix_env.SignedBinary(
      target=output_directory_name + '/' + msi_name,
      source=wix_output,
  )

  return env.Replicate(output_dir + '/' + output_directory_name, sign_output)
