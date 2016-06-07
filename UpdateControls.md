# Overview #
Omaha allows administrators to control how and when Omaha updates applications. See the [announcement](http://google-opensource.blogspot.com/2009/05/google-update-releases-update-controls.html) on the Open Source Blog. The documentation for administrators is in the [Google Update for Enterprise documentation](http://www.google.com/support/installer/go/enterprise).

# Implementation #
Omaha's Group Policy support is implemented by reading the appropriate [registry settings](http://www.google.com/support/installer/go/enterprise#Registry_Settings) from `HKLM\Software\Policies\Google\Update` when performing specific actions. For example, when determining whether it should do an update check, Omaha attempts to read `AutoUpdateCheckPeriodMinutes`. If found, that value is used instead of the default value for the comparison with (now - `LastChecked`).

The Administrative Template is automatically generated for all [supported applications](http://www.google.com/support/installer/bin/answer.py?answer=146158) by scripts in the [enterprise](http://code.google.com/p/omaha/source/browse/#svn/trunk/enterprise) directory.