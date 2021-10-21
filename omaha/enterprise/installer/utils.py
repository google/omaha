"""Helper methods and classes for creating enterprise installers.

This module separates out some basic build_enterprise_installer.py
functionality, allowing it to be used in both scons and "standard" python code.
"""

import binascii
from datetime import date
import md5


_GOOGLE_UPDATE_NAMESPACE_GUID = 'BE19B3E4502845af8B3E67A99FCDCFB1'


def ConvertToMSIVersionNumberIfNeeded(product_version):
  """Change product_version to fit in an MSI version number if needed.

  Some products use a 4-field version numbering scheme whereas MSI looks only
  at the first three fields when considering version numbers. Furthermore, MSI
  version fields have documented width restrictions of 8bits.8bits.16bits as
  per http://msdn.microsoft.com/en-us/library/aa370859(VS.85).aspx

  As such, the following scheme is used:

  Product a.b.c.d -> MSI X.Y.Z:
    X = (1 << 6) | ((C & 0xffff) >> 10)
    Y = (C >> 2) & 0xff
    Z = ((C & 0x3) << 14) | (D & 0x3FFF)

  So eg. 6.1.420.8 would become 64.105.8

  This assumes:
  1) we care about neither the product major number nor the product minor
     number, e.g. we will never reset the 'c' number after an increase in
     either 'a' or 'b'.
  2) 'd' will always be <= 16383
  3) 'c' is <= 65535

  We assert on assumptions 2) and 3)

  Args:
    product_version: A version string in "#.#.#.#" format.

  Returns:
    An MSI-compatible version string, or if product_version is not of the
    expected format, then the original product_version value.
  """

  try:
    version_field_strings = product_version.split('.')
    (build, patch) = [int(x) for x in version_field_strings[2:]]
  except:  # pylint: disable=bare-except
    # Couldn't parse the version number as a 4-term period-separated number,
    # just return the original string.
    return product_version

  # Check that the input version number is in range.
  assert patch <= 16383, 'Error, patch number %s out of range.' % patch
  assert build <= 65535, 'Error, build number %s out of range.' % build

  msi_major = (1 << 6) | ((build & 0xffff) >> 10)
  msi_minor = (build >> 2) & 0xff
  msi_build = ((build & 0x3) << 14) | (patch & 0x3FFF)

  return str(msi_major) + '.' + str(msi_minor) + '.' + str(msi_build)


def GetInstallerNamespace():
  return binascii.a2b_hex(_GOOGLE_UPDATE_NAMESPACE_GUID)


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
  md5_hex_digest = mymd5.hexdigest()
  md5_hex_digits = [md5_hex_digest[x:x+2].upper()
                    for x in range(0, len(md5_hex_digest), 2)]

  # Set various reserved bits to make this a valid GUID.

  # "Set the four most significant bits (bits 12 through 15) of the
  # time_hi_and_version field to the appropriate 4-bit version number
  # from Section 4.1.3."
  version = int(md5_hex_digits[6], 16)
  version = 0x30 | (version & 0x0f)

  # "Set the two most significant bits (bits 6 and 7) of the
  # clock_seq_hi_and_reserved to zero and one, respectively."
  clock_seq_hi_and_reserved = int(md5_hex_digits[8], 16)
  clock_seq_hi_and_reserved = 0x80 | (clock_seq_hi_and_reserved & 0x3f)

  return (
      '%s-%s-%02X%s-%02X%s-%s' % (
          ''.join(md5_hex_digits[0:4]),
          ''.join(md5_hex_digits[4:6]),
          version,
          md5_hex_digits[7],
          clock_seq_hi_and_reserved,
          md5_hex_digits[9],
          ''.join(md5_hex_digits[10:])))


def GetWixCandleFlags(
    product_name, product_name_legal_identifier, msi_product_version,
    product_version, product_guid,
    company_name=None,
    custom_action_dll_path=None,
    product_uninstaller_additional_args=None,
    msi_product_id=None,
    msi_upgradecode_guid=None,
    product_installer_path=None,
    product_installer_data=None,
    product_icon_path=None,
    product_installer_install_command=None,
    product_installer_disable_update_registration_arg=None,
    product_custom_params=None,
    standalone_installer_path=None,
    metainstaller_path=None,
    architecture=None):
  """Generate the proper set of defines for WiX Candle usage."""
  flags = [
      '-dProductName=' + product_name,
      '-dProductNameLegalIdentifier=' + product_name_legal_identifier,
      '-dProductVersion=' + msi_product_version,
      '-dProductOriginalVersionString=' + product_version,
      '-dProductBuildYear=' + str(date.today().year),
      '-dProductGuid=' + product_guid,
      ]

  if company_name:
    flags.append('-dCompanyName=' + company_name)

  if custom_action_dll_path:
    flags.append('-dMsiInstallerCADll=' + custom_action_dll_path)

  if product_uninstaller_additional_args:
    flags.append('-dProductUninstallerAdditionalArgs=' +
                 product_uninstaller_additional_args)

  if msi_product_id:
    flags.append('-dMsiProductId=' + msi_product_id)

  if msi_upgradecode_guid:
    flags.append('-dMsiUpgradeCode=' + msi_upgradecode_guid)

  # This is allowed to be an empty string.
  if product_custom_params is not None:
    flags.append('-dProductCustomParams="%s"' % product_custom_params)

  if standalone_installer_path:
    flags.append('-dStandaloneInstallerPath=' + standalone_installer_path)

  if metainstaller_path:
    flags.append('-dGoogleUpdateMetainstallerPath="%s"' % metainstaller_path)

  if product_installer_install_command:
    flags.append('-dProductInstallerInstallCommand=' +
                 product_installer_install_command)

  if product_installer_disable_update_registration_arg:
    flags.append('-dProductInstallerDisableUpdateRegistrationArg=' +
                 product_installer_disable_update_registration_arg)

  if product_installer_path:
    flags.append('-dProductInstallerPath=' + product_installer_path)

  if product_installer_data:
    product_installer_data = product_installer_data.replace(
        '==MSI-PRODUCT-ID==', msi_product_id)
    flags.append('-dProductInstallerData=' + product_installer_data)

  if product_icon_path:
    flags.append('-dProductIcon=' + product_icon_path)

  if architecture:
    # Translate some common strings, like from platform.machine().
    arch_map = {
        'amd64': 'x64',
        'x86_64': 'x64',
    }
    flags.extend(['-arch', arch_map.get(architecture, architecture)])
  return flags


def GetWixLightFlags():
  # Disable warning LGHT1076 and internal check ICE61 on light.exe.
  # Details:
  # http://blogs.msdn.com/astebner/archive/2007/02/13/building-an-msi-using-wix-v3-0-that-includes-the-vc-8-0-runtime-merge-modules.aspx
  # http://windows-installer-xml-wix-toolset.687559.n2.nabble.com/ICE61-Upgrade-VersionMax-format-is-wrong-td4396813.html
  return ['-sw1076', '-sice:ICE61']
