# Command line options #

Omaha builds using Hammer from Google's open-sourced [Software Construction Toolkit](http://code.google.com/p/swtoolkit/). This page covers some of the command line options that may be useful for building.

|hammer|Default build (debug, using precompiled headers, signed with test certificate, most subdirectories)|
|:-----|:--------------------------------------------------------------------------------------------------|
|hammer MODE=dbg-win|Debug build                                                                                        |
|hammer MODE=opt-win|Optimized build                                                                                    |
|hammer MODE=all|Debug, optimized, and coverage builds                                                              |
|hammer --clean|Clean intermediate files (`hammer -c` also works)                                                  |
|hammer --verbose|Verbose output                                                                                     |
|hammer --use\_precompiled\_headers|Enable precompiled header support (default)                                                        |
|hammer --no-use\_precompiled\_headers|Disable precompiled header support                                                                 |
|hammer --min|Minimum build (useful for quickly testing changes in goopdate.dll)                                 |
|hammer --all|Complete build (all subdirectories)                                                                |
|hammer --authenticode\_file=`<file>`|Use `<file>` as signing key. Must use .pfx file.                                                   |
|hammer --authenticode\_password=`<password>`|`<password>` is the password for the signing key.                                                  |
|hammer --msvs|Generate a Visual Studio solution and projects                                                     |
|hammer omaha\_unittest.exe|Build only the unit tests (useful for quickly unit-testing changes)                                |

Command line options can be combined, so the command:
`hammer MODE=opt-win --min --verbose`
will do a minimum optimized build, producing verbose output.