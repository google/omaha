# Introduction #

These instructions are intended to assist the would-be Omaha developer with setting up an environment in which to successfully build the Omaha source code. The open source build of Omaha is not hermetic. That means that several dependencies are needed to be resolved by hand in order to have a tree which can be built.

We are striving to make the code build with the latest Windows toolchain from Microsoft. Since there is no continuous integration for this project, the code may not build using previous versions of the toolchain.

#### Currently, the supported toolchain is Visual Studio 2022 Update 17.8.3 and Windows SDK 10.0.22621.0. ####

The updater runs on Windows 7, 8, and 10. Windows XP is not supported in the current build configuration due to a number of issues, such as thread-safe initializing of static local variables, etc.

# Required Downloads/Tools #

The following packages are required to build Omaha:
  * A copy of the Omaha source code.  This can be done by cloning this repository.
  * Microsoft Visual Studio 2022. The free Visual Studio Community edition is sufficient to build.
    * Download [here](https://visualstudio.microsoft.com/downloads)
  * Windows 10 SDK.
    * Visual Studio copy of Windows 10 SDK is sufficient to build with, if desired.
    * Optionally, download and intall Windows 10 SDK [here](https://dev.windows.com/en-us/downloads/windows-10-sdk).
  * The Windows Template Library (WTL) - WTL 10.0.10320 Release
    * Download WTL [here](http://sourceforge.net/projects/wtl/).
    * hammer.bat has `OMAHA_WTL_DIR` set to `C:\wtl\files`. Change this if you unpacked to a different location.
  * The Windows Install XML (WiX) Toolkit, version 3.0 or later.
    * Download any of the v3 binaries packages [here](http://wix.sourceforge.net/).
    * Set the `WIX` environment variable to the directory where you unpacked WiX.
  * Python 2.7.x
    * Download Python [here](https://www.python.org/downloads/release/python-2716).  It can coexist with newer Python installs on a system.
    * You'll also need the pywin32 (Python for Windows) extensions for Python 2.7.
      - You can install with pip: `> python -m pip install pywin32` - assuming `python` is added to your `PATH` environmental variable.
      - It can also be downloaded [here](https://github.com/mhammond/pywin32/releases/download/b224/pywin32-224.win-amd64-py2.7.exe).
    * The `OMAHA_PYTHON_DIR` is set to `C:\Python27`. Change this if you installed to a different location.
  * SCons 1.3.x (Be sure to use **1.3**, the 2.0 series is not backwards-compatible!)
    * Download SCons [here](http://sourceforge.net/projects/scons/files/scons/1.3.1/).
    * Change this line in hammer.bat if you installed to a different location: `SCONS_DIR=C:\Python27\scons-1.3.1`.
  * Google Software Construction Toolkit
    * Get the SCT source [here](https://code.google.com/archive/p/swtoolkit/downloads), either via direct download or via SVN checkout.
    * Change this line in hammer.bat if you installed to a different location: `set SCT_DIR=C:\swtoolkit`.
  * The GO programming language
    * Download [here](https://golang.org/dl/) 
    * Change this line in hammer.bat if you installed to a different location: `set GOROOT=C:\go`.
  * Google Protocol Buffers (currently tested with v3.17.3) [here](https://github.com/protocolbuffers/protobuf/releases).
    * From the [release page](https://github.com/protocolbuffers/protobuf/releases), download the zip file `protoc-$VERSION-win32.zip`. It contains the protoc binary. Unzip the contents under `C:\protobuf`. After that, download the zip file `protobuf-cpp-$VERSION.zip`. Unzip the `src` sub-directory contents to `C:\protobuf\src`. If other directory is used, please edit the environment variables in the hammer.bat, specifically, `OMAHA_PROTOBUF_BIN_DIR` and `OMAHA_PROTOBUF_SRC_DIR`.
  * Third-party dependencies:
    * breakpad. Download [here](https://github.com/google/breakpad/archive/refs/heads/main.zip). Tested with commit [11ec9c](https://github.com/google/breakpad/commit/11ec9c32888c06665b8838f709bd66c0be9789a6) from Dec 11, 2023.
      - Unzip everything inside `breakpad-master.zip\breakpad-master` to `third_party\breakpad`.
    * googletest. Download [here](https://github.com/google/googletest/archive/refs/heads/master.zip). Tested with commit [96eadf
](https://github.com/google/googletest/commit/96eadf659fb75ecda943bd97413c71d4c17c4f43) from Dec 22, 2023. This includes both gtest and gmock frameworks.
      - Unzip everything inside `googletest-master.zip\googletest-master` to `third_party\googletest`.
    * libzip 1.7.3. Source code [here](https://libzip.org/download/libzip-1.7.3.tar.xz). Unzip the contents of `libzip-1.7.3.tar.gz\libzip-1.7.3.tar\libzip-1.7.3\` into the directory `third_party\libzip`. The Omaha repository contains two generated configuration files in `base\libzip`, or one could build the libzip library and generate the files. A change has been made to config.h to disable zip crypto `#undef HAVE_CRYPTO`, or else the zip code won't build because of a compile time bug.
    * zlib 1.2.11. Source code [here](https://zlib.net/zlib-1.2.11.tar.gz). Unzip the contents of `zlib-1.2.11.tar.gz\zlib-1.2.11.tar\zlib-1.2.11\` into the directory `third_party\zlib`.

To run the unit tests, one more package is needed. Download the Windows Sysinternals PSTools suite [here](https://technet.microsoft.com/en-us/sysinternals/bb897553) and save psexec.exe somewhere. Then, set a system environment variable named OMAHA_PSEXEC_DIR to the directory containing psexec.exe.

# Details #

## Installation ##

 * Install each of the above software packages that comes with an installer.
 * Make sure you add the installed Python directory to your path environment variable.
 * Follow the Software Construction Toolkit [installation instructions](http://code.google.com/p/swtoolkit/wiki/Introduction). Make sure you don't miss the part about setting up the SCONS_DIR environment variable.
 * Get the source code for the packages that don't have an installer.
 * Clone the Omaha repository to a location of your choice. The structure should look similar to:
```
      D:\src\omahaopensource\omaha>ls -l
      total 16
      ----rwx---+ 1 sorin Domain Users 752 Jul 14 12:27 README.md
      d---rwx---+ 1 sorin Domain Users   0 Jun 30 17:58 common
      d---rwx---+ 1 sorin Domain Users   0 Jul 15 11:34 omaha
      d---rwx---+ 1 sorin Domain Users   0 Jun 30 17:58 third_party

      d:\src\omahaopensource\omaha>ls -l third_party
      drwxrwxrwx 1 sorin sorin 4096 Mar  1 19:37 breakpad
      drwxrwxrwx 1 sorin sorin 4096 Mar  1 19:41 googletest
      drwxrwxrwx 1 sorin sorin 4096 Mar  1 19:58 libzip
      drwxrwxrwx 1 sorin sorin 4096 Mar  1 16:30 lzma
      drwxrwxrwx 1 sorin sorin 4096 Mar  1 20:07 zlib
```

## Environment Variables ##

Once the tree is in place, a number of environment variables need to be set up in the file ```omaha\\hammer.bat``` to match the local build environment. The sample of ```omaha\\hammer.bat``` is provided as an example and assumes an x64 architecture for the OS.

## Build ##

Once the above setup is complete:
 * Open a fresh cmd.exe window as Administrator.
 * Navigate to the 'omaha' directory in your source checkout, for example: `D:\src\omahaopensource\omaha\omaha>`
 * Run the vsvars.bat file corresponding to the Visual C++ instance you want to use to build. For example: `%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsamd64_x86.bat`. This step sets up the environment variables that the build scripts use. The 64-bit host toolchain is preferred, otherwise, run `vcvars32.bat` to set up the build environment for the x86 host toolchain.
 * From the above directory, just type `hammer` to build Omaha! (Note: More advanced build options can be found in [HammerOptions](HammerOptions.md).  In particular, if you are building on a multi-core or multi-processor, consider passing the -j# flag to Hammer to enable parallel compilation.)
 * To build all targets in all modes, type `hammer --all --mode=all`. This builds both debug and opt versions of the binaries, including all unit tests, and standalone installers.

Hammer should build Omaha, and then run a very limited set of unit tests, which must pass in order to complete the build.
A larger suite of unit tests is also included in the Omaha source.


## Running Unit Tests ##

The Omaha build process includes building an automated unit test suite, based on the [GTest](https://github.com/google/googletest) framework.  In order to run it, there are two pieces of preparation you must do:

* Create the following registry key: `HKEY_LOCAL_MACHINE\SOFTWARE\OmahaCompanyName\UpdateDev`. Then, add a string value named `TestSource` with the value `ossdev`. (Note: If you are on 64 bit Windows and are using `regedit` to create the value then you need to place it in `HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\OmahaCompanyName\UpdateDev`. [This allows 32 bit processes to read it.](https://support.microsoft.com/en-us/kb/305097)).
* Download the Windows Sysinternals PSTools suite (available [here](http://technet.microsoft.com/en-us/sysinternals/bb897553)) and save `psexec.exe` somewhere. Then, set an environment variable named `OMAHA_PSEXEC_DIR` to the directory containing `psexec.exe`.

When running unit tests:
* You must be connected to the Internet for some tests to pass.
* We recommend running them with administrator privileges, as some tests do not run otherwise.
* Some tests do not run by default, because they take a long time or are otherwise inconvenient to run all the time. To run these tests, define the `OMAHA_RUN_ALL_TESTS` environment variable. For example, `set OMAHA_RUN_ALL_TESTS=1`.

To run the unit test suite, run the following executable after a successful build:

`>scons-out\dbg-win\staging\omaha_unittest.exe`


## Testing Omaha Against Google Servers ##

By default, the public version of Omaha continues to point to google.com for downloads and update checks.  You can test the operation of an unmodified client by attempting to download and install a Google product, using your build of Omaha against the official Google Update servers.  For example:

* Google Chrome (run as admin):
> > `scons-out\dbg-win\staging\GoogleUpdate.exe /install "bundlename=Google%20Chrome%20Bundle&appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%20Chrome&needsadmin=True&lang=en"`
* Google Talk Bundle (run as normal user):
> > `scons-out\dbg-win\staging\GoogleUpdate.exe /install "bundlename=Google%20Talk%20Bundle&appguid={D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}&appname=Google%20Talk%20Plugin&needsadmin=False&lang=en"`

The client should contact google.com for a version query, then download and install the product.  (However, you can expect it to fail shortly after install with the error code `0x80040905`.  This is due to these products writing their registration information to the Google Update area of the registry, instead of your Omaha's location.)
