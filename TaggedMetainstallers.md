# Introduction #

The quoted string that appears after `/install` and `/handoff` in Omaha command lines is called the _tag_. By embedding this tag in the metainstaller, users can just run or double-click on the metainstaller to install the specified app.

## Background ##

There is a section of an Authenticode signature that can be modified without affecting the signature. We call this area the tag and use it for the purposes described above.

The steps on this page only apply to files with Authenticode signatures. By default, the metainstaller is signed using a self-signed certificate. This metainstaller is named `GoogleUpdateSetup.exe` and can be found in the `staging` directory.

# Applying a Tag to the Metainstaller #

A tool called `ApplyTag.exe` is built as part of the normal build. You will need this tool and the metainstaller (`GoogleUpdateSetup.exe`) from `staging`.

The first step is to determine the tag you want to apply. You can use any combination of the extra arguments listed in `const_cmd_line.h`. The argument names must be followed by an equals sign (`=`) and the value. Arguments are concatenated with the ampersand (`&`). `appguid` must precede the other values you want to apply to that app. Some values apply to all apps regardless of where they appear.

Below is an example tag to install My App per-user in English.
> `appguid={C38B7539-CD23-4954-BDC8-FCE21B2543AE}&appname=My%20App&needsadmin=False&lang=en`

Now you are ready to tag the metainstaller. `ApplyTag.exe` shows the syntax. An example is below.
> `ApplyTag.exe GoogleUpdateSetup.exe MyAppSetup.exe "appguid={C38B7539-CD23-4954-BDC8-FCE21B2543AE}&appname=My%20App&needsadmin=False&lang=en"`

The tag must be quoted in the command line, but the double quotes are not included in the tag. You can confirm the tag by opening `MyAppSetup.exe` in Notepad.

> Note: To append to an existing tag, add `append` to the end of the command line.

# Using the Tagged Metainstaller #

Double-click or otherwise run `MyAppSetup.exe`. The metainstaller code reads the tag, adds double quotes, and appends this string to "`/install `". It then launches `GoogleUpdate.exe` with this command line. You should see Omaha launch with the title "My App Installer" and proceed to install your application.