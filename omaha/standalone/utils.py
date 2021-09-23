"""Helper methods and classes for creating standalone installers.

This module separates out some basic standalone_installer.py functionality,
allowing it to be used in both scons-based and "standard" python code.
"""

import array
import base64
import hashlib
import os


def GenerateUpdateResponseFile(target, sources, version_list, has_x64_binaries):
  """Generate GUP file based on a list of sources.

  Args:
    target: Target GUP file name.
    sources: A list of source files. Source files should be listed as manifest1,
      binary1, manifest2, binary2 and so on. Order is important so that
      manifests and installers can be differentiated and 'INSTALLER_VERSIONS'
      can be applied properly.
    version_list: A list of versions for corresponding binaries in sources and
      should be in same order.
    has_x64_binaries: Sets the 'ARCH_REQUIREMENT' replacement to 'x64' rather
      than 'x86' if True.

  Raises:
    Exception: When build encounters error.
  """
  xml_header = '<?xml version="1.0" encoding="UTF-8"?>\n'
  response_header = '<response protocol="3.0">'
  response_footer = '</response>'
  arch_requirement = 'x86'
  if has_x64_binaries:
    arch_requirement = 'x64'

  manifest_content_list = [xml_header, response_header]
  for file_index in range(0, len(sources), 2):
    source_manifest_path = sources[file_index]
    binary_path = sources[file_index + 1]
    size = os.stat(os.path.abspath(binary_path)).st_size
    data = array.array('B')
    installer_file = open(os.path.abspath(binary_path), mode='rb')
    data.fromfile(installer_file, size)
    installer_file.close()
    sha256 = hashlib.sha256()
    sha256.update(data)
    hash_value = sha256.hexdigest()

    manifest_file = open(os.path.abspath(source_manifest_path))
    manifest_content = manifest_file.read()
    manifest_file.close()
    response_body_start_index = manifest_content.find('<response')
    if response_body_start_index < 0:
      raise Exception('GUP file does not contain response element.')
    # + 1 to include the closing > in header
    response_body_start_index = manifest_content.find(
        '>', response_body_start_index)
    if response_body_start_index < 0:
      raise Exception('GUP file does not contain response element.')
    response_body_start_index += 1
    response_body_end_index = manifest_content.find(
        '</response>', response_body_start_index)
    if response_body_end_index < 0:
      raise Exception('GUP file is not in valid response format.')
    resp = manifest_content[response_body_start_index:response_body_end_index]
    resp = resp.replace('${INSTALLER_SIZE}', str(size))
    resp = resp.replace('${INSTALLER_HASH_SHA256}', hash_value)
    resp = resp.replace('${INSTALLER_VERSION}', version_list[int(file_index/2)])
    resp = resp.replace('${ARCH_REQUIREMENT}', arch_requirement)
    manifest_content_list.append(resp)
  manifest_content_list.append(response_footer)

  manifest_content_str = ''.join(manifest_content_list)
  output_file = open(os.path.abspath(target), 'w')
  output_file.write(manifest_content_str)
  output_file.close()


def WriteInstallerLog(log_fn, log_text, manifest_fn):
  """Save a log of what went into the installer."""
  dump_data = ''
  file_to_dump = open(os.path.abspath(manifest_fn), 'r')
  content = file_to_dump.read()
  file_to_dump.close()
  dump_data += '\nMANIFEST:\n'
  dump_data += str(manifest_fn)
  dump_data += '\n'
  dump_data += content
  f = open(os.path.abspath(log_fn), 'w')
  f.write(log_text)
  f.write(dump_data)
  f.close()
  return 0
