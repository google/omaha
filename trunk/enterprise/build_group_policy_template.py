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

"""A Hammer-specific wrapper for generate_group_policy_template."""

from omaha.enterprise import generate_group_policy_template


def BuildGroupPolicyTemplate(env, target, apps, apps_file_path=None):
  """Builds a Group Policy ADM template file, handling dependencies.

  Causes WriteGroupPolicyTemplate() to be called at build time instead of as
  part of the processing stage.

  Args:
    env: The environment.
    target: ADM output file.
    apps: A list of tuples containing information about each app. See
        generate_group_policy_template for details.
    apps_file_path: Optional path to the file that defines apps. Used to enforce
        dependencies.
  """

  def _WriteAdmFile(target, source, env):
    """Called during the build phase to generate and write the ADM file."""
    source = source  # Avoid PyLint warning.
    generate_group_policy_template.WriteGroupPolicyTemplate(
        env.File(target[0]).abspath,
        env['public_apps'])
    return 0

  adm_output = env.Command(
      target=target,
      source=[],
      action=_WriteAdmFile,
      public_apps=apps
  )

  # Force ADM file to rebuild whenever the script or apps data change.
  dependencies = ['$MAIN_DIR/enterprise/generate_group_policy_template.py']
  if apps_file_path:
    dependencies.append(apps_file_path)
  env.Depends(adm_output, dependencies)
