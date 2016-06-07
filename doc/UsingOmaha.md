# Introduction #

Once you've built Omaha, you may be wondering what to do next. This page describes several potential next steps. If you haven't already, [run the Omaha unit tests](DeveloperSetupGuide#Running_Unit_Tests.md) before proceeding.

# Test Omaha With Google Applications #

If you're just getting started and want to verify that Omaha works, you can try to install a Google application with the client you just built.

To force your build to install and be used, you must either remove existing Google Update instances or make sure the version specified in http://code.google.com/p/omaha/source/browse/trunk/VERSION is newer than the version of Google Update that is installed.

There are several sample command lines at the top of http://code.google.com/p/omaha/source/browse/trunk/goopdate/goopdate.cc. For the initial install of any build, you should use one of the `/install` command lines. `needsadmin` determines whether to install Omaha and the app per-user (`False`) or per-machine/for all users (`True`).

As an example, from the `staging` directory, run the following command:
> `GoogleUpdate.exe /install "appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&appname=Google%20Chrome&needsadmin=False&lang=en"`

This will install your build in Documents & Settings (XP) / Users (Vista), and it will in turn download and install Google Chrome.

The /silent flag can be used to install with no user interaction. /silent needs to come before /install. For a tagged metainstaller, the command line would be of the form:
> `GoogleAppsStandaloneSetup.exe /silent /installsource foo /install`

# Customize Omaha for Your Application/Organization #

**Note: Before you can use Omaha for your own application(s), you must have an update server that supports the Omaha ServerProtocol.** Google does not currently provide an update server for external applications.

See CustomizingOmaha for information on customizing the Omaha client to work with your server and applications.

# Create a Metainstaller for Your Application #

The quoted string after `/install` in the command line above is called the tag. This tag can be added to the metainstaller (`GoogleUpdateSetup.exe`) so that double-clicking the metainstaller will install the application specified by the tag.

See TaggedMetainstallers.