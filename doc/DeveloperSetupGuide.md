# Introduction #

These instructions are intended to assist the would-be Omaha developer with setting up an environment in which to successfully build the Omaha source code. The open source build of Omaha is not hermetic. That means that several dependencies are needed to be resolved by hand in order to have a tree which can be built.

We are striving to make the code build with the latest Windows toolchain from Microsoft. Since there is no continuous integration for this project, the code may not build using previous versions of the toolchain.

#### Currently, the supported toolchain is Visual Studio 2015 Update 3 and Windows SDK 10.0.10586.0.####

# Required Downloads/Tools #

The following packages are required to build Omaha:
  * A copy of the Omaha source code.  This can be done by cloning this repository.
  * Microsoft Visual Studio 2015 Update 3. The free Visual Studion Community edition is sufficient to build.
   * The Express Editions are not sufficient - they do not include ATL/MFC headers or libraries, which Omaha requires.
  * ATL Server headers 
   * Download [here](http://atlserver.codeplex.com). Omaha needs this library for regular expression support.
  * Windows 10 SDK.
   * Download Windows 10 SDK [here](https://dev.windows.com/en-us/downloads/windows-10-sdk).
  * Microsoft .NET Framework 2.0
   * This should be pre-installed on Windows Vista and Windows 7. This old version of SDK is needed for click-once compatibility with Windows XP systems.
   * To verify, see if the file %WINDIR%\Microsoft.NET\Framework\v2.0.50727\csc.exe exists on your system.
   * Download [here](https://www.microsoft.com/en-us/download/details.aspx?id=19988).
  * The Windows Template Library (WTL)
   * Download WTL [here](http://sourceforge.net/projects/wtl/).
  * The Windows Install XML (WiX) Toolkit, version 3.0 or later.
   * Download any of the v3 binaries packages [here](http://wix.sourceforge.net/).
  * Python 2.4.x (Be sure to use **2.4**, newer versions currently break the build!)
   * Download Python [here](http://www.python.org/download/releases/2.4.4/).  It can coexist with newer Python installs on a system.
   * You'll also need the pywin32 (Python for Windows) extensions for Python 2.4.  It can be downloaded [here](http://sourceforge.net/projects/pywin32/files/pywin32/Build216/pywin32-216.win32-py2.4.exe/download).
  * SCons 1.3.x (Be sure to use **1.3**, the 2.0 series is not backwards-compatible!)
   * Download SCons [here](http://sourceforge.net/projects/scons/files/scons/1.3.1/).
  * Google Software Construction Toolkit
   * Get the SCT source [here](http://code.google.com/p/swtoolkit/), either via direct download or via SVN checkout.
  * The GO programming language
   * Download [here](https://golang.org/dl/) 
  * Third-party dependecies:
   * breakpad. Source code [here](https://code.google.com/p/google-breakpad/source/checkout)
   * googletest. Source code [here](https://github.com/google/googletest). This includes both gtest and gmock frameworks.
   * Use git clone, git svn clone, or other way to get the source code for these projects into the third_party directory in the root of this repository.

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
      total 16
      d---rwx---+ 1 sorin          Domain Users 0 Jul 14 12:52 breakpad
      drwxrwx---+ 1 Administrators Domain Users 0 Sep  1 11:52 googletest
      d---rwx---+ 1 sorin          Domain Users 0 Aug  7 18:58 lzma
```

## Environment Variables ##

Once the tree is in place, a number of environment variables need to be set up in the file ```omaha\\hammer.bat``` to match the local build environment. The sample of ```omaha\\hammer.bat``` is provided as an example and assumes an x64 architecture for the OS.

## Build ##

Once the above setup is complete:
 * Open a fresh cmd.exe window as Administrator.
 * Navigate to the 'omaha' directory in your source checkout, for example: `D:\src\omahaopensource\omaha\omaha>`
 * Run the vsvars.bat file corresponding to the Visual C++ instance you want to use to build. For example: `%ProgramFiles(x86)%\Microsoft Visual Studio 14.0\VC\vcvarsall.bat`. This step sets up the environment variables that the build scripts use.
 * From the above directory, just type `hammer` to build Omaha! (Note: More advanced build options can be found in [HammerOptions](HammerOptions.md).  In particular, if you are building on a multi-core or multi-processor, consider passing the -j# flag to Hammer to enable parallel compilation.)
 * To build all targets in all modes, type `hammer --all --mode=all`. This builds both debug and opt versions of the binaries, including all unit tests, and standalone installers.

Hammer should build Omaha, and then run a very limited set of unit tests, which must pass in order to complete the build.
A larger suite of unit tests is also included in the Omaha source.


## Running Unit Tests ##

The Omaha build proces includes building an automated unit test suite, based on the [GTest](https://github.com/google/googletest) framework.  In order to run it, there are two pieces of preparation you must do:

* Create the following registry key: `HKEY_LOCAL_MACHINE\SOFTWARE\OmahaCompanyName\UpdateDev`. Then, add a string value named `TestSource` with the value `ossdev`. (Note: If you are on 64 bit Windows and are using `regedit` to create the value then you need to place it in `HKEY_LOCAL_MACHINE\Wow6432Node\SOFTWARE\OmahaCompanyName\UpdateDev`. [This allows 32 bit processes to read it.](https://support.microsoft.com/en-us/kb/305097)).
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
