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

"""Runs a set of unit tests and returns success only if they all succeed.

This script assumes it is being run from the omaha directory.

To run unit tests for Omaha's default set of test directories, just run the file
from the command line.
"""


import dircache
import os

TEST_EXECUTABLE_RHS = '_unittest.exe'

# Build paths that contain tests.
STAGING_PATH = 'scons-out\\dbg-win\\staging'
TESTS_PATH = 'scons-out\\dbg-win\\tests'


def RunTest(test_path):
  """Runs a test and returns its exit code.

    Assumes the tests can be run from any directory. In other words, it does not
    chdir.

  Args:
    test_path: Path to test executables.

  Returns:
    The exit code from the test process.
  """

  print '\nRunning %s . . .\n' % test_path

  # Put './' in front of the file name to avoid accidentally running a file with
  # the same name in some other directory if test_path were just a file name.
  return os.system(os.path.join('.', test_path))


def RunTests(test_paths):
  """Runs all tests specified by test_paths.

  Args:
    test_paths: A list of paths to test executables.

  Returns:
    0 if all tests are successful.
    1 if some tests fail, or if there is an error.
  """

  if not test_paths or len(test_paths) < 1:
    return 1

  print 'Found the following tests to run:'
  for test in test_paths:
    print '\t%s' % test

  # Run all tests and remembers those that failed.
  failed_tests = [t for t in test_paths if RunTest(t)]

  print '\n\n%s test executables were run.' % len(test_paths)

  failed_test_count = len(failed_tests)
  if failed_test_count:
    # Lists the executables that failed so the user can investigate them.
    print 'FAILED!'
    print 'The following %s tests failed:\n' % failed_test_count
    for test in failed_tests:
      print test
    return 1
  else:
    # No, there is none.
    if test_paths:
      print 'All of them PASSED!'
    return 0


def GetTestsInDirs(test_dirs):
  """Returns a list of all unit test executables in test_dirs.

    Does not search subdirectories.

  Args:
    test_dirs: A list of directories to search.

  Returns:
    List of all unit tests.
  """

  tests = []

  for test_dir in test_dirs:
    # Use dircache.listdir so order is alphabetical and thus deterministic.
    files = dircache.listdir(test_dir)

    for test_file in files:
      if test_file.endswith(TEST_EXECUTABLE_RHS):
        relative_path = os.path.join(test_dir, test_file)
        if os.path.isfile(relative_path):
          tests += [relative_path]

  return tests

# Run a unit test when the module is run directly.
if __name__ == '__main__':
  # List of paths that contain unit tests to run.
  dirs_containing_tests = [STAGING_PATH, TESTS_PATH]

  tests_to_run = GetTestsInDirs(dirs_containing_tests)

  RunTests(tests_to_run)
