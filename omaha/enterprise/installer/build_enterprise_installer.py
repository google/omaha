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
  BuildEnterpriseInstaller(): Build an MSI installer for use in enterprises.
"""

import enterprise.installer.utils as ei_utils




def BuildGoogleUpdateFragment(env,
                              metainstaller_path,
                              company_name,
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
    company_name: name of the company the fragment is being built for
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
  msi_product_version = ei_utils.ConvertToMSIVersionNumberIfNeeded(
      product_version)

  product_name_legal_identifier = product_name.replace(' ', '')

  intermediate_base_name = wixobj_base_name + '_google_update_fragment'

  copy_target = env.Command(
      target=intermediate_base_name + '.wxs',
      source=google_update_wxs_template_path,
      action='@copy /y $SOURCE $TARGET',
  )

  wix_defines = ei_utils.GetWixCandleFlags(
      '"%s"' % product_name,
      '"%s"' % product_name_legal_identifier,
      msi_product_version,
      product_version,
      '"%s"' % product_guid,
      '"%s"' % company_name,
      product_custom_params=product_custom_params,
      metainstaller_path=str(env.File(metainstaller_path).abspath))

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
                    custom_action_dll_path,
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
    custom_action_dll_path: path to the custom action dll that
        exports a ShowInstallerResultUIString and ExtractTagInfoFromInstaller
        methods. ShowInstallerResultUIString reads the
        LastInstallerResultUIString from the product's ClientState key in
        the registry and display the string via MsiProcessMessage.
        ExtractTagInfoFromInstaller extracts brand code from tagged MSI
        package.
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

  msi_product_version = ei_utils.ConvertToMSIVersionNumberIfNeeded(
      product_version)

  omaha_installer_namespace = ei_utils.GetInstallerNamespace()

  # Include the .msi filename in the Product Code generation because "the
  # product code must be changed if... the name of the .msi file has been
  # changed" according to http://msdn.microsoft.com/en-us/library/aa367850.aspx.
  msi_product_id = ei_utils.GenerateNameBasedGUID(
      omaha_installer_namespace,
      'Product %s %s' % (product_name, msi_base_name)
  )
  msi_upgradecode_guid = ei_utils.GenerateNameBasedGUID(
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
      WIXCANDLEFLAGS=ei_utils.GetWixCandleFlags(
          product_name,
          product_name_legal_identifier,
          msi_product_version,
          product_version,
          product_guid,
          custom_action_dll_path=str(env.File(custom_action_dll_path).abspath),
          product_uninstaller_additional_args=product_uninstaller_additional_args,
          msi_product_id=msi_product_id,
          msi_upgradecode_guid=msi_upgradecode_guid,
          product_installer_path=str(env.File(product_installer_path).abspath),
          product_installer_install_command=product_installer_install_command,
          product_installer_disable_update_registration_arg=(
              product_installer_disable_update_registration_arg)),
      WIXLIGHTFLAGS=ei_utils.GetWixLightFlags()
  )

  wix_output = wix_env.WiX(
      target='unsigned_' + msi_name,
      source=[copy_target, google_update_wixobj_output],
  )

  # Force a rebuild when the installer or metainstaller changes.
  # The metainstaller change does not get passed through even though the .wixobj
  # file is rebuilt because the hash of the .wixobj does not change.
  # Also force a dependency on the CA DLL. Otherwise, it might not be built
  # before the MSI.
  wix_env.Depends(wix_output, [product_installer_path,
                               metainstaller_path,
                               custom_action_dll_path])

  sign_output = wix_env.SignedBinary(
      target=msi_name,
      source=wix_output,
  )

  env.Replicate(output_dir, sign_output)


def BuildEnterpriseInstaller(env,
                             company_name,
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
                             custom_action_dll_path,
                             metainstaller_path,
                             output_dir='$STAGING_DIR'):
  """Build an installer for use in enterprise situations.

  Builds an MSI using the supplied details and binaries. This MSI is
  intended to enable enterprise installation scenarios.

  Args:
    env: environment to build with
    company_name: name of the company for whom the product is being built
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
    custom_action_dll_path: path to the custom action dll that
        exports a ShowInstallerResultUIString and ExtractTagInfoFromInstaller
        methods. ShowInstallerResultUIString reads the
        LastInstallerResultUIString from the product's ClientState key in
        the registry and display the string via MsiProcessMessage.
        ExtractTagInfoFromInstaller extracts brand code from tagged MSI
        package.
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
      company_name,
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
      custom_action_dll_path,
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
    custom_action_dll_path,
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
    custom_action_dll_path: path to the custom action dll that
        exports a ShowInstallerResultUIString and ExtractTagInfoFromInstaller
        methods. ShowInstallerResultUIString reads the
        LastInstallerResultUIString from the product's ClientState key in
        the registry and display the string via MsiProcessMessage.
        ExtractTagInfoFromInstaller extracts brand code from tagged MSI
        package.
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
  msi_product_version = ei_utils.ConvertToMSIVersionNumberIfNeeded(
      product_version)

  omaha_installer_namespace = ei_utils.GetInstallerNamespace()

  # Include the .msi filename in the Product Code generation because "the
  # product code must be changed if... the name of the .msi file has been
  # changed" according to http://msdn.microsoft.com/en-us/library/aa367850.aspx.
  # Also include the version number since we process version changes as major
  # upgrades.
  msi_product_id = ei_utils.GenerateNameBasedGUID(
      omaha_installer_namespace,
      'Product %s %s %s' % (product_name, msi_base_name, product_version)
  )
  msi_upgradecode_guid = ei_utils.GenerateNameBasedGUID(
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
  wix_candle_flags = ei_utils.GetWixCandleFlags(
      product_name,
      product_name_legal_identifier,
      msi_product_version,
      product_version,
      '"%s"' % product_guid,
      product_custom_params=product_custom_params,
      standalone_installer_path=str(env.File(standalone_installer_path).abspath),
      custom_action_dll_path=str(env.File(custom_action_dll_path).abspath),
      product_uninstaller_additional_args=product_uninstaller_additional_args,
      msi_product_id=msi_product_id,
      msi_upgradecode_guid=msi_upgradecode_guid,
      product_installer_data=product_installer_data)

  wix_env.Append(
      WIXCANDLEFLAGS=wix_candle_flags,
      WIXLIGHTFLAGS=ei_utils.GetWixLightFlags()
  )

  wix_output = wix_env.WiX(
      target = output_directory_name + '/' + 'unsigned_' + msi_name,
      source = [copy_target],
  )

  # Force a rebuild when the standalone installer changes.
  # The metainstaller change does not get passed through even though the .wixobj
  # file is rebuilt because the hash of the .wixobj does not change.
  # Also force a dependency on the CA DLL. Otherwise, it might not be built
  # before the MSI.
  wix_env.Depends(wix_output, [standalone_installer_path,
                               custom_action_dll_path])

  sign_output = wix_env.SignedBinary(
      target=output_directory_name + '/' + msi_name,
      source=wix_output,
  )

  return env.Replicate(output_dir + '/' + output_directory_name, sign_output)
