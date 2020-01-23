# Omaha3 Source Organization #
The Omaha client is split along its modes - each task that goopdate is capable of performing (i.e. running as the Update3 COM server, or as a Client, or a bootstrapper, or as an Omaha2 COM server, etc.) is split out into a different directory.  Thus, we have the following subdirectories that each produce a .lib as their output in the build:
```
omaha\
        base\           Headers\utilities used by multiple Google products.
        common\         Headers\utilities that are Omaha-specific, used throughout the product.
        ui\             UI-specific code, used by the client and setup libraries.
        net\            Network code, used by the COM server.
        statsreport\    Metric collection and reporting code. (Used by both client and server.)
        setup\          Implementation of client setup code. (Make permanent installs of Omaha)
        client\         Implementation of client install code. (Contact and utilize the COM server)
        service\        Utility code related to the Update3 COM server.
        core\           Implements the “core process” - an alternate server mode mostly used
                        for Omaha2 backwards compatibility and crash handling/reporting.
```
All of the static libraries produced by these directories feed into code in this one:
```
omaha\
        goopdate\       Contains the bulk of the Update3 COM server code, plus the goopdate 
                        entry point that handles command line parsing and calls into client modes.
                        The main output of this directory is goopdate.dll, although it also builds
                        the COM proxy DLLs and the forwarder EXEs.
```
There’s also a few free-standing binaries produced in subdirectories, most of which consume base.lib and common.lib:
```
omaha\
        goopdate\
                resources\      Produces a separate resource-only DLL for each of the languages
                                that Omaha ships in.
        google_update\          Produces the constant shell, GoogleUpdate.exe.
        mi_exe_stub\            Produces a stub EXE, mi_exe_stub.exe, that will be combined
                                with a TAR to produce the untagged meta-installer.  (The script to
                                actually do the merge lives in installers\, mentioned below.)
	recovery\               Produces tools for “Code Red” - a mechanism that the apps being
                                managed by Omaha can use to check Omaha’s integrity, and
                                restore it if it appears broken.
```
Finally, we have a few directories that are filled with tools for generating installers and test tools once Omaha proper has been successfully built:
```
omaha\
        enterprise\             Produces GoogleUpdate.adm, an optional file that can be used 
                                by system administrators to manage Omaha via Group Policy.
        installers\             Produces GoogleUpdateSetup.exe, the meta-installer, using a
                                set of Python scripts to generate the Omaha tarball and merging
                                it with mi_exe_stub.exe.  (This build step also produces some
                                of the Code Red files.)
        clickonce\              Produces clickonce_bootstrap.exe -- a managed stub, written in
                                C#, used to enable ClickOnce installs via the .NET Framework.
        data\                   Certificates for self-signing test builds.
        testing\                Builds omaha_unittest.exe, our unit tester.
```