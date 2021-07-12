# An Omaha Walkthrough #

You might want to read this in parallel with the design docs; they explain the intent and design of Omaha3, while this doc is an overview of the current implementation.

## Common Terminology ##

  * “User/Machine Omaha” - Omaha can be installed in two different ways on a system, and may even have multiple installed copies on a single system simultaneously:
    * User Omaha: Omaha is installed for a single user.  The files live in the user’s local application data directory (for example, `\Users\%user%\AppData\Local\Google\Update` on Win7), and all relevant Registry keys are stored in `HKCU\Software\Google`.  Omaha installs are initiated by a user; subsequent automatic updates are run under that user’s account.
    * Machine Omaha: Omaha is installed in a machine-wide manner.  The files live in the machine’s app directory -- for example, `\Program Files (x86)\Google\Update` -- and all relevant Registry keys are stored in `HKLM\Software\Google\Update` (for 32-bit) or `HKLM\Software\Wow6432Node\Google\Update` (for 64-bit).  Omaha installs are still typically initiated by a user account, but the automatic updates themselves will run under `LocalSystem`.
  * “Goopdate” - The core DLL in which nearly all Omaha code lives.
  * “Constant Shell” - The executable commonly known as `GoogleUpdate.exe`.  This is a small, statically linked EXE with no load-time DLL dependencies (not even a CRT!).  It attempts to find a copy of `goopdate.dll` -- the local directory first, if one exists there, otherwise the Omaha install location in the registry.  It may verify that the DLL has an intact Authenticode signature from Google, and then loads the DLL and runs its Main() function, passing along any command line switches it received.
  * “Meta-installer” (often abbreviated to MI) - The executable known as `GoogleUpdateSetup.exe`.  A metainstaller is an EXE with a large binary resource containing a compressed TAR archive of all the files needed for an Omaha install.  The MI’s code decompresses and extracts the tarball’s contents to a temporary directory, and runs the constant shell inside that directory, passing any command line along.  The MI is almost always Authenticode signed.
  * “Tagged Meta-installer” - A copy of the MI that has been configured to pass a predefined command line to Omaha - for example, `ChromeSetup.exe`.  A “tag” refers to a predefined command line.  We stow the tag string in an PE section that is not covered by the Authenticode signature; in this way, a meta-installer EXE can be modified to download and install nearly any app that Google offers by simply adding the tag, without having to re-sign it.
  * “CUP” -  A proprietary Google protocol, built on top of HTTP.  It provides SSL-like authentication/repudiation while avoiding some of the downsides of HTTPS.  Acronym for “Client Update Protocol.”  CUP is described in more detail in a [separate design document](cup.html).
  * “Code Red” - Describes a situation where a bugged Omaha build or an OS patch has been released that prevents Omaha from updating for a very large percentage of users, and the tools for resolving it.  Google-authored applications can link with a static library produced during the Omaha build that checks for Code Red releases through an alternate channel; if a Code Red release is enabled, an executable is downloaded which attempts to repair/reinstall Omaha.

## Core Design Breakout ##

A working Omaha install on Windows consists of three fundamental components:

```
(------------ user machine ------------)              |    (---- operated by google ----)
[Omaha Client] <--COM RPC--> [Update3 COM Server] <--network--> [Omaha Update Server]
```

The Omaha Update Server implementation is outside of this document's scope.  For the purposes of this doc, you should know that it is an HTTP(S) server, written in Java and running on Google servers, which accepts a HTTP(S) POST request with an XML-formatted body and responds with XML-formatted data.  (It can also use the CUP protocol.)

(Note: In addition to Omaha v3 requests, the Update Server also handles requests from Omaha v1 and v2, which used different XML schemas, as well as requests from non-Omaha entities such as Macintosh clients.)

The Omaha COM Server operates on a customer’s machine, at whatever privilege level that the install requires - `LocalSystem` for machine-wide installs, or the user account for per-user installs.  It maintains a state machine for all applications it currently tracks (exposed as a COM object) and worker threads that will advance the state machine automatically in response to certain function calls.  It handles all network access and launches all installers, impersonating the client where necessary; as a general rule, it has no UI.  It is the heart of Omaha.

(Note: There are actually multiple COM servers supported by Goopdate, which will expose different objects based on the command line used to start them.  However, the above, which is known as the Update3 server, is the only one you should generally care about.)

The Omaha Client always operates at user privilege levels and owns the UI of Omaha.  It has two duties:
  * Setup - Create or update a permanent Omaha install, of either user or machine variety.
  * Install - Invoke the COM server to create a state machine object, fill it out with apps to be managed, and call a suitable function on it such as `checkForUpdate()`, `download()`, or `install()`.  From that point onwards, poll the state object as the COM server does the work for you, and update the UI as the states advance.

In general, when referring to “the client”, we’re referring to the official Google Update client, which happens to live in the same executable as the COM Server; the role that the executable plays is decided simply by which command line is passed to it.  However, there are other clients that may access the COM server; The server must stay as secure as possible, and sanitize all input.

## Example Code Flow ##

Let’s walk through an example - the user downloads a Chrome installer from a machine that has no version of Omaha at all.  What actually happens:

  * The Chrome installer EXE (~500KB) is actually a tagged meta-installer.  It decompresses the Omaha files to a temporary directory and invokes the constant shell with the following command line.  The part after the /install is the tag string:
```
GoogleUpdate /install "bundlename=Google%20Chrome%20Bundle&appguid={8A69D345-D564-463C-AFF1-A69D9E530F96}&
appname=Google%20Chrome&needsadmin=False&lang=en"
```
  * The constant shell loads goopdate.dll from the temporary directory and passes along this command line; we start by parsing it.  The `/install` flag tells us that we’re operating as the Client, and we need to install Omaha; we examine and sanitize the tag (known from here on as extra-args) to figure out whether we need a user or machine Omaha.  We decide from `“needsadmin=false”` that a user Omaha install is warranted.  (If needsadmin were equal to true, we would attempt to re-launch ourselves in a new, elevated process.)
  * We check the machine to see if there’s already a user Omaha installed with a version newer than or equal to ours.  (If it’s equal to ours, we will do some supplementary checking to make sure that the installed copy is sane and working properly, and if not, we over-install.)  Let’s assume that there is no user Omaha installed.  We will create the direct directory in `AppData`, copy over the files, and then make entries in the Registry to do the following:
    * Register our COM servers
    * Create scheduled tasks to check for an update every five hours
    * Store initial configuration/state for Omaha itself in the Registry
    * Register Omaha itself as an Omaha-managed application, so it can check for updates for itself
  * The client then starts a new copy of itself in its permanent installed location, modifying the command line from `/install` to `/handoff`.  Once again, the constant shell loads Goopdate and passes the command line along - this time, however, we’re using the constant shell in the newly-created permanent install of Omaha, rather than the one in the temp directory.
  * This time, the command line is `/handoff` - we decide that we’re a Client again, but now we assume that we’re running from a permanently installed copy, and are being asked to to download and install an app from the network.  The Client uses COM to create a copy of an Update3 object called the AppBundle, which is implemented on the our COM server.  Windows will start the COM server (which is done by running the constant shell, again - but this time, with a different command line, triggering Server mode) and returns to the client a `AppBundle*` - which is actually a COM RPC proxy for an AppBundle in the COM server process.
  * The client adds the supplied GUID from the tag to the AppBundle, and asks the COM server to check for an update.  The COM server spins up a worker thread which will manipulate the app bundle; it checks the machine for an existing install of that app (let’s assume it finds none), then formats an XML request and sends it securely to the Update Server over the network.
  * The Google-managed server will respond with an XML response that contains a URL to download the Chrome installer, its size, and hash.  The COM server parses this, stores the info in the App Bundle, then shifts its state into “Ready to Download”.  The client, which has been polling it and watching the status, calls `download()` on the bundle - which triggers the worker thread to start downloading it.  Using a similar handoff of polling and invoking methods, the client will order the server to cache the downloaded installer, verify its integrity, and start the installer.
  * The Chrome installer, which is managed by the Chrome team, installs Chrome.  As part of its install, it writes data to Omaha’s area of the Registry, registering itself as being managed by Omaha.
  * Finally, the Chrome installer process exits.  The COM Server sends a ping (an XML request formatted with the success/failure of the Chrome installer, as denoted by the EXE’s exit code, plus supplemental info about download duration and the like) to the Update Server.  The COM client releases the AppBundle and exits.
  * The COM Server no longer has any active clients, and will shut down on its own in a minute or two.
  * Five hours later, a scheduled task fires that starts up the constant shell with the commandline `/ua`.  This triggers a Client who reads the registry to get a full list of all applications currently being managed by Omaha.  It creates an App Bundle (starting the COM Server) and adds all the apps to it; the COM Server checks for updates, and the whole process starts again.

A crucial thing to pick up here is that, since one file (goopdate.dll) does many different tasks based simply on the command line, a typical Omaha task will involve many processes being created.  A typical clean install of Machine Chrome may involve as many as 15 different instances of GoogleUpdate.exe being started, each doing small parts of the task.

## The Omaha Files ##

So, what files are actually in a permanent install of Omaha once it’s completed?

| Filename | Description |
|:-------------------|:--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `GoogleUpdate.exe` | **The Constant Shell.**  Just takes the command line given to it and passes it to goopdate.dll; if necessary, it will validate that goopdate has an intact digital signature from Google. |
| `GoogleCrashHandler.exe` | A copy of the constant shell, renamed for Omaha2 compatibility reasons.  Expected to always be started with /crashhandler. |
| `GoogleUpdateBroker.exe`<br>`GoogleUpdateOnDemand.exe` | **COM Forwarders**.  Both of these are small EXEs whose sole purpose is to take their own command line, append a command line switch to the end, and pass it to the Constant Shell. 
| `goopdate.dll` |  The central Omaha3 binary. |                                                                                             
| `goopdateres_*.dll` | **Resource-only DLLs, one per language, containing localized strings**. As part of its startup, Goopdate will read the “lang” extra-args parameter if one exists (or the Registry) and select a language to load. |
| `psmachine.dll`<br>`psuser.dll` | **Custom marshaling stubs used by the COM Server**. Used in order to work around some Windows bugs that are triggered by having both Machine and User Omaha installed simultaneously.|


The directory tree typically looks like this:
```
Google\
    Update\
        1.3.21.53\              The install location for the current version of Omaha.
            ... the files listed above ...
        Download\		Temp area for installers currently being downloaded.
        Install\		Temp area for installers that are verified and about to be launched.
        GoogleUpdate.exe	A copy of the the constant shell.  This will look into the registry for
				the most recently successfully installed version of Omaha, and
				use the goopdate.dll there.
```
At this point, the value of the Constant Shell becomes obvious - we can modify or change the location of goopdate.dll, without having to touch <code>GoogleUpdate.exe</code> in most cases.  This means that minor changes or bugfixes in Omaha can be pushed out, in the form of an update to goopdate.dll, without triggering a prompt from firewalls, virus scanners, or process whitelisters that may be in place on a machine.
