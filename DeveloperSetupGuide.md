# Introduction #

These instructions are intended to assist the would-be Omaha developer with setting up an environment in which to successfully build the Omaha source code.


# Required Downloads/Tools #

The following packages are required to build Omaha:
  * A copy of the Omaha source code.  This can be
    * You can either:
      * Download a full source package from the [Downloads area](http://code.google.com/p/omaha/downloads/list).
      * See [source checkout](http://code.google.com/p/omaha/source/checkout) for setting up access to the subversion repository. This will also require [SVN](http://subversion.tigris.org/) itself.
    * However you get the source code, it must be checked out or extracted to a directory named `omaha` exactly.
  * Microsoft Visual C++
    * You will need to have at least the Standard Edition of Visual C++ or Visual Studio, 2005 or later.
      * The Express Editions are not sufficient - they do not include ATL/MFC headers or libraries, which Omaha requires.
      * If you use Visual Studio 2008 or 2010, you will need an additional download.  (In these versions, the ATL Regex library was removed from the standard ATL/MFC headers and moved into the open source ATL Server project.)
        * Download the ATL Server headers [here](http://atlserver.codeplex.com).
  * Microsoft Vista SDK
    * Download the SDK [here](http://www.microsoft.com/download/en/details.aspx?displaylang=en&id=11310).
  * Microsoft .NET Framework 2.0
    * This should be pre-installed on Windows Vista and Windows 7.
    * To verify, see if the file %WINDIR%\Microsoft.NET\Framework\v2.0.50727\csc.exe exists on your system.
    * If not, you can download the installer [here](http://msdn.microsoft.com/en-us/netframework/aa731542.aspx).
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


# Details #

## Installation ##

  1. Install each of the above software packages that comes with an installer, which is all of them except Omaha source code and the Software Construction Toolkit.
  1. Make sure you add the installed python directory to your path environment variable.
  1. Follow the Software Construction Toolkit [installation instructions](http://code.google.com/p/swtoolkit/wiki/Introduction). Make sure you don't miss the part about setting up the SCONS\_DIR environment variable.
  1. Extract or checkout the Omaha source code to a location of your choice, perhaps something like "C:\omaha".

## Environment Variables ##

Create the following environment variables:
  * SCT\_DIR - set to the directory of the Software Construction Toolkit (eg. `C:\swtoolkit`)
  * OMAHA\_NET\_DIR - This will depend on your OS:
    * On Windows Vista or if it was pre-installed on Windows XP, set it to the framework directory (something like `C:\Windows\Microsoft.NET\Framework\v2.0.50727`).
    * Otherwise, set it to the directory where the .NET framework is installed.  (eg. `C:\Program Files\Microsoft.NET\Framework\v2.0.50727`)
  * OMAHA\_WTL\_DIR - set to the include directory in the WTL installation (eg. `C:\Program Files\WTL\include`)
  * OMAHA\_WIX\_DIR - set to the directory in WiX where 'candle.exe' and 'light.exe' are installed. This may be something like (eg. `C:\Program Files\Windows Installer XML v3\bin`)
  * OMAHA\_VISTASDK\_DIR - set to the directory where the Vista SDK was installed (e.g. `C:\Program Files\Microsoft SDKs\Windows\v6.0`)
  * OMAHA\_PYTHON\_DIR - set to the directory where python was installed. This directory should contain python.exe. (eg. `C:\python_24`)

## Setup the environment variables for the installed version of Visual Studio ##

For the build script to pick up the correct version of Visual Studio, it is usually necessary to call a script provided with Visual Studio. For instance, in the case of Visual Studio 2005, the name of the script is `vcvarsall.bat`; for 2008 and later, the name is `vcvars32.bat`.  If you are using 2008 or 2010, also make sure that the ATL Server headers have been downloaded and are added to the compiler include path.

It's highly recommended that you combine all of the above preparations in a batch file that can be reused.  For example:

```
@echo off
rem -- Set all environment variables used by Hammer and Omaha. --
set OMAHA_NET_DIR=%WINDIR%\Microsoft.NET\Framework\v2.0.50727
set OMAHA_WTL_DIR=%ProgramFiles%\WTL\include
set OMAHA_WIX_DIR=%ProgramFiles%\Windows Installer XML v3.5\bin
set OMAHA_VISTASDK_DIR=%ProgramFiles%\Microsoft SDKs\Windows\v6.1
set OMAHA_PSEXEC_DIR=%ProgramFiles%\PSTools
set OMAHA_PYTHON_DIR=C:\Python244
set SCONS_DIR=%OMAHA_PYTHON_DIR%\Lib\site-packages\scons-1.3.1
set SCT_DIR=C:\swtoolkit
rem -- Add Visual Studio and Python to our path, and set VC env variables. --
call "%ProgramFiles%\Microsoft Visual Studio 9.0\VC\bin\vcvars32.bat"
path %OMAHA_PYTHON_DIR%;%PATH%
rem -- Add ATLServer headers to the Visual C++ include path. --
set INCLUDE=%INCLUDE%C:\atlserver\include;
```

## Build ##

Once the above setup is complete:
  1. Open a fresh cmd.exe window (if you're running under Vista, make sure cmd.exe is running as Administrator)
  1. Navigate to the 'Omaha' directory, or whatever you called it.
  1. From the above directory, just type `hammer` to build Omaha! (Note: More advanced build options can be found in [HammerOptions](HammerOptions.md).  In particular, if you are building on a multi-core or multi-processor, consider passing the -j# flag to Hammer to enable parallel compilation.)

Hammer should build Omaha, and then run a very limited set of unit tests, which must pass in order to complete the build.

A larger suite of unit tests is also included in the Omaha source.

## Common Build Errors ##

  * **When I attempt to run '`hammer`', I get an error message saying that \hammer.bat can't be found.**
    * This means that the build scripts can't find the Software Construction Toolkit (SCT).
      * Fix this by making sure that `SCT_DIR` and all the `OMAHA_` environment variables listed above are set.

  * **I get a build break in `precompile.c`, saying that it can't find `atlrx.h`.**
    * This means that you're using a recent version of Visual Studio, made after Microsoft forked off ATL Server, and your copy of the MFC headers is missing this file.
      * Fix it by downloading and extract the ATL Server headers, and make sure that the path to them is in the `INCLUDE` environment variable.

  * **I get the following error messages while building:**
```
  "SignTool Error: The specified timestamp server could not be reached."
  "SignTool Warning: Signing succeeded, but an error occurred while attempting to timestamp: %FILE%"
```
    * This means that the Microsoft code signing tool is having trouble contacting timestamp.verisign.com for countersigning.
      * Fix it by ensuring that your build system has a functioning Internet connection and resuming the build.

  * **The build appears to finish successfully, but when it runs the post-build unit tests, the `OmahaCustomizationTest.IsInternalUser` test fails and the build aborts.**
    * This means that your build machine's DNS name and/or AD domain doesn't match the domain/company name set in main.scons.
      * This can be fixed in one of two ways:
        1. Move your build system onto your corporate network.
        1. Set the environment variable `OMAHA_TEST_BUILD_SYSTEM=1` to disable this test.

  * **The build appears to finish successfully, but when it runs the post-build unit tests, all of the `ClientUtilsTest` tests fail and the build aborts.**
    * This means that you've changed the company or product name strings in main.scons, but haven't changed the localized string resources to match.
      * Fix this by updating the string resources in the following locations to have up-to-date company names:
        * `goopdate\resources\goopdateres\*.rc`
        * `goopdate\resources\goopdate_dll\*.rc`
        * `mi_exe_stub\resources\*.rc`

## Running Unit Tests ##

The Omaha build proces includes building an automated unit test suite, based on the [GTest](http://code.google.com/p/googletest/) framework.  In order to run it, there are two pieces of preparation you must do:

  1. Create the following registry key: `HKEY_LOCAL_MACHINE\SOFTWARE\Google\UpdateDev`. Then, add a string value named `TestSource` with the value `ossdev`.
  1. Download the Windows Sysinternals PSTools suite (available [here](http://technet.microsoft.com/en-us/sysinternals/bb897553)) and save `psexec.exe` somewhere. Then, set an environment variable named `OMAHA_PSEXEC_DIR` to the directory containing `psexec.exe`.

When running unit tests:
  * You must be connected to the Internet for some tests to pass.
  * We recommend running them with administrator privileges, as some tests do not run otherwise.
  * Some tests do not run by default, because they take a long time or are otherwise inconvenient to run all the time. To run these tests, define the `OMAHA_RUN_ALL_TESTS` environment variable. For example, `set OMAHA_RUN_ALL_TESTS=1`.

To run the unit test suite, run the following executable after a successful build:

> `scons-out\dbg-win\staging\omaha_unittest.exe`

Note that there are a few unit tests that are expected to fail on an unmodified build of Omaha.  These are:

  * `AppManagerReadAppPersistentDataMachineTest.ReadUninstalledAppPersistentData_UninstalledApp`
  * `AppManagerReadAppPersistentDataMachineTest.ClientStateExistsWithoutPvOrClientsKey`
  * `AppManagerReadAppPersistentDataUserTest.AppExists_NoDisplayName`
  * `AppManagerReadAppPersistentDataUserTest.AppExists_EmptyDisplayName`
  * `AppManagerReadAppPersistentDataUserTest.ReadUninstalledAppPersistentData_UninstalledApp`
  * `AppManagerReadAppPersistentDataUserTest.ReadUninstalledAppPersistentData_EulaNotAccepted`
  * `AppManagerReadAppPersistentDataUserTest.ClientStateExistsWithoutPvOrClientsKey`
  * `AppManagerReadAppPersistentDataUserTest.ClientStateExistsWithEmptyPvNoClientsKey`
    * These depend on localization changes.  They will disappear once you update the string resources to match your company name.

  * `InstallManagerInstallAppMachineTest.InstallApp_MsiInstallerSucceeds`
  * `InstallManagerInstallAppMachineTest.InstallApp_MsiInstallerWithArgumentSucceeds`
  * `InstallerWrapperMachineTest.InstallApp_MsiInstallerSucceeds`
  * `InstallerWrapperMachineTest.InstallApp_MsiInstallerWithArgumentSucceeds`
    * These tests use a small MSI installer in `testing\unittest_support` that is hardwired to write some data to the the Google Update registry paths.  The WiX sources for this MSI are in `test\test_foo.wxs.xml` -- update this to contain your company and product name, build the new MSI, and drop it into `unittest_support`.

## Common Unit Test Issues ##

  * **The following tests are failing:**
```
   SetupUserTest.TerminateCoreProcesses_BothTypesRunningAndSimilarArgsProcess
   SetupMachineTest.TerminateCoreProcesses_BothTypesRunningAndSimilarArgsProcess
```
    * This means that `psxec.exe` can't be found.
      * Fix it by making sure that the environment variable `OMAHA_PSEXEC_DIR` contains a valid path to your install of PSTools.

  * **The `HighresTimer.SecondClock` test is failing.**
    * This test is not reliable on virtual machines.
      * Fix it by allocating higher priorities to the virtual CPU performing the build, or running the unit tests on a physical CPU.

## Testing Omaha Against Google Servers ##

By default, the public version of Omaha continues to point to google.com for downloads and update checks.  You can test the operation of an unmodified client by attempting to download and install a Google product, using your build of Omaha against the official Google Update servers.  For example:

  * Google Chrome (run as admin):
> > `scons-out\dbg-win\staging\GoogleUpdate.exe /install "bundlename=Google%20Chrome%20Bundle&appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%20Chrome&needsadmin=True&lang=en"`
  * Google Talk Bundle (run as normal user):
> > `scons-out\dbg-win\staging\GoogleUpdate.exe /install "bundlename=Google%20Talk%20Bundle&appguid={D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}&appname=Google%20Talk%20Plugin&needsadmin=False&lang=en"`

The client should contact google.com for a version query, then download and install the product.  (However, you can expect it to fail shortly after install with the error code `0x80040905`.  This is due to these products writing their registration information to the Google Update area of the registry, instead of your Omaha's location.)

You can check [UsingOmaha](UsingOmaha.md) for more detailed examples.

## Converting Omaha to Google Update ##

The version of Omaha on Subversion has a few changes from the official Google Update release to prevent forks from overwriting official data.  These are:

  * Different COM CLSIDs, IIDs, and ProgIDs
  * Different company/product name strings (used to generate Registry locations)
  * Different prefixes for shared Win32 object names

If you would like to convert Omaha to Google Update, you can do this by copying the contents of the `official\` subdirectory on top of the Omaha source code.