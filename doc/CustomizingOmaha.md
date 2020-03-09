# Introduction #

When initially checked out, the open-source Omaha builds "Google Update".  Google Update communicates with Google's update servers, which **only support Google applications**.

The main difference between code compiled from this site and Google Update is the Authenticode signature and version number, and the CLSIDs of the COM objects. (To restore the original CLSIDs and build a complete Google Update clone, see the files in the official/ folder of the source.)

In order to use Omaha for your application or organization, you must customize the source code for your organization, and prevent it from conflicting with Google Update (or other forks of Omaha) on users' computers.  You must also create your own update server.

# Mandatory Changes #

The following items **MUST** be changed before releasing a fork of Omaha.  Preferably, make these changes as soon as you start development:

  * **`omaha\main.scons`**

> Ensure that **`is_google_update_build`** is set to False, and adjust the vendor-specific constant strings.  ("Google", "Update", etc.)  In particular, once you have your update server functional, be sure to change the domain from google.com to your company's server.

> Most of those constant strings will be used to generate preprocessor defines, which are then used to create other internal strings -- for example, Registry locations, COM ProgIDs and cross-process mutex names -- so it is _**crucial**_ that you make these strings unique.

  * **`omaha\base\const_object_names.h`**

> Modify **`kGlobalPrefix`** at the top of the file to contain your company name.

  * **`omaha\common\const_goopdate.h`**

> Modify the names of the service names (examples: **`omaha_task_name_c`**, **`omaham_service_name`**, etc.) to contain your product's name.

  * **`omaha\base\const_addresses.h`**

> If necessary, modify the URLs that will be used for performing update checks and returning results to match your update server's implementation.

  * **`omaha\goopdate\omaha3_idl.idl`**

> Generate new GUIDs for every interface and coclass.  Changing the descriptive names for them isn't a bad idea either.  (Do not, however, change code-level names such as `IAppBundle` or `GoogleUpdate3UserClass`.)


> Generate new GUIDs for every interface and coclass.

  * **`omaha\goopdate\resources\goopdate_dll\*.rc`**
  * **`omaha\goopdate\resources\goopdateres\*.rc`**
  * **`omaha\mi_exe_stub\*.rc`**

> Update each translation to reflect your company and product.  Omaha stores all its localized data in a set of resource-only DLLs, one per language. (The MetaInstaller stub, on the other hand, stores a far more minimal set of translations for all languages inside its EXE.)

# Recommended Changes #

We strongly recommend making these changes before you release:

  * Change the names of core executables -- `GoogleUpdate.exe`, `goopdate.dll`, `GoogleCrashHandler.exe`, and so on.  (This will require changes to the SCons build scripts, to the names of EXEs in `omaha\base\constants.h`, to WiX MSI fragments, and to several unit tests.)

  * Duplicate the contents of `common\omaha_customization_unittest.cc` into a new file, replace **`EXPECT_GU_STREQ()`** with **`EXPECT_STREQ()`**, and update the string literals for your fork of Omaha.  Do the same with **`EXPECT_GU_ID_EQ()`** in a copy of `goopdate\omaha_customization_goopdate_apis_unittest.cc`.  (These macros change their behavior based on the value of `is_google_update_build` in `main.scons`.  When set to `True`, this test checks for equality - i.e. that the values haven't changed between builds.  When set to `False`, it checks for inequality - i.e. it ensures that these values do not collide with the official Google Update releases.  By adding additional tests with the first behavior, you can provide the same safety measures for your own fork.)

  * Define a new CUP public key if you plan to use the CUP protocol on your update server.

  * If your product exposes any settings that are controlled via Group Policy, you may want to move the Omaha GPO settings to the same location as your product.  This is set in `omaha\common\const_group_policy.h`.

  * Change the value of the reference GUID used to identify your fork of Omaha when self-updating.  This is defined in **`kGoopdateGuid`** and **`GOOPDATE_APP_ID`** in `omaha\base\constants.h` and verified in several unit tests.

  * Do a general search for strings that reference Google or Google Update, and revise them.  This should include debug logging statements.

# Versioning #

The version number stored in all outputs is set in the file **`omaha\VERSION`**.  Omaha has some functionality in it from Google Update related to bug workarounds when upgrading from prior versions, so don't set the VERSION to any lower than 1.3.23.0.

When releasing your fork of Omaha, we recommend starting the version at 1.3.25.0.  Remember to bump the version up whenever releasing an updated version.
