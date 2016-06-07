# Omaha Client-Server Protocol V3 #

This document describes version 3 of the Omaha client-server protocol.  Omaha launched on Windows with this version of the protocol in May 2011.  (Version 2 of the protocol launched in May 2007 on Windows and May 2008 on Mac; Version 1 of the protocol was never deployed publicly.)

Version 2 is documented [here](ServerProtocolV2.md).

## Overview ##
The client sends requests via HTTP POST with an XML data body.  The response is an XML data body.

![http://omaha.googlecode.com/svn/wiki/ServerProtocol_Images/omahaprotocol3.png](http://omaha.googlecode.com/svn/wiki/ServerProtocol_Images/omahaprotocol3.png)

An HTTP request may concatenate multiple applications in one XML body; similarly, multiple request-actions may be included for any application.  The server responds with a status and other information (as appropriate) for each action, organized in a similar nested XML structure.

#### Custom HTTP Headers ####
The Client can attach one or more custom HTTP headers to the POST request.  They are purely advisory in nature, and their presence and content are not required in order to provide responses to update checks.

## Examples ##
### Update Check ###
This example also shows bundling of requests for two different applications.

_Request:_

```
<?xml version="1.0" encoding="UTF-8"?>
<request protocol="3.0" version="1.3.23.0" ismachine="0" sessionid="{5FAD27D4-6BFA-4daa-A1B3-5A1F821FEE0F}" userid="{D0BBD725-742D-44ae-8D46-0231E881D58E}" installsource="scheduler" testsource="ossdev" requestid="{C8F6EDF3-B623-4ee6-B2DA-1D08A0B4C665}">
  <hw sse2="1"/>
  <os platform="win" version="6.1" sp="" arch="x64"/>
  <app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" version="1.3.23.0" nextversion="" lang="en" brand="GGLS" client="someclientid" installage="39">
    <updatecheck/>
    <ping r="1"/>
  </app>
  <app appid="{D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}" version="2.2.2.0" nextversion="" lang="en" brand="GGLS" client="" installage="6">
    <updatecheck/>
    <ping r="1"/>
  </app>
</request>
```

_Response (negative):_

There is not an update for either of the apps. The app and ping elements are acknowledged with status="ok".
```
<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0" server="prod">
  <daystart elapsed_seconds="56508"/>
  <app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" status="ok">
    <updatecheck status="noupdate"/>
    <ping status="ok"/>
  </app>
  <app appid="{D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}" status="ok">
    <updatecheck status="noupdate"/>
    <ping status="ok"/>
  </app>
</response>
```

_Response (positive):_

There is an update available for only the second app. The app and ping elements are acknowledged with status="ok".
```
<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0" server="prod">
  <daystart elapsed_seconds="56508"/>
  <app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" status="ok">
    <updatecheck status="noupdate"/>
    <ping status="ok"/>
  </app>
  <app appid="{D0AB2EBC-931B-4013-9FEB-C9C4C2225C8C}" status="ok">
    <updatecheck status="ok">
      <urls>
        <url codebase="http://cache.pack.google.com/edgedl/chrome/install/782.112/"/>
      </urls>
      <manifest version="13.0.782.112">
        <packages>
          <package hash="VXriGUVI0TNqfLlU02vBel4Q3Zo=" name="chrome_installer.exe" required="true" size="23963192"/>
        </packages>
        <actions>
          <action arguments="--do-not-launch-chrome" event="install" run="chrome_installer.exe"/>
          <action version="13.0.782.112" event="postinstall" onsuccess="exitsilentlyonlaunchcmd"/>
        </actions>
      </manifest>
    </updatecheck>
    <ping status="ok"/>
  </app>
</response>
```

### Event Report ###
Events are reports from the client to the server; no server response data are required, but they should be acknowledged with status="ok".

_Request:_
```
<?xml version="1.0" encoding="UTF-8"?>
<request protocol="3.0" version="1.3.23.0" ismachine="1" sessionid="{2882CF9B-D9C2-4edb-9AAF-8ED5FCF366F7}" userid="{F25EC606-5FC2-449b-91FF-FA21CADB46E4}" installsource="otherinstallcmd" testsource="ossdev" requestid="{164FC0EC-8EF7-42cb-A49D-474E20E8D352}">
  <os platform="win" version="6.1" sp="" arch="x64"/>
  <app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" version="" nextversion="13.0.782.112" lang="en" brand="" client="" installage="6">
    <event eventtype="9" eventresult="1" errorcode="0" extracode1="0"/>
    <event eventtype="5" eventresult="1" errorcode="0" extracode1="0"/>
    <event eventtype="2" eventresult="4" errorcode="-2147219440" extracode1="268435463"/>
  </app>
</request>
```

_Response:_
```
<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0" server="prod">
  <daystart elapsed_seconds="56754"/>
  <app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" status="ok">
    <event status="ok"/>
    <event status="ok"/>
    <event status="ok"/>
  </app>
</response>
```

### Data Request ###
Install data are requested at install time; the updatecheck in this example has been removed for clarity.

_Request:_
```
<?xml version="1.0" encoding="UTF-8"?>
<request protocol="3.0" version="1.3.23.0" ismachine="0" sessionid="{5FAD27D4-6BFA-4daa-A1B3-5A1F821FEE0F}" userid="{D0BBD725-742D-44ae-8D46-0231E881D58E}" installsource="scheduler" testsource="ossdev" requestid="{C8F6EDF3-B623-4ee6-B2DA-1D08A0B4C665}">
  <os platform="win" version="6.1" sp="" arch="x64"/>
  <app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" version="1.3.23.0" nextversion="" lang="en" brand="GGLS" client="someclientid" installage="39">
    <updatecheck/>
    <data name="install" index="verboselogging"/>
    <data name="untrusted">Some untrusted data</data>
    <ping r="1"/>
  </app>
</request>
```

_Response:_
```
<?xml version="1.0" encoding="UTF-8"?>
<response protocol="3.0" server="prod">
  <daystart elapsed_seconds="56754"/>
  <app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" status="ok">
    <data index="verboselogging" name="install" status="ok">
      app-specific values here
    </data>
    <data name="untrusted" status="ok"/>
  </app>
</response>
```

## Reference ##
A terse description of the XML elements and attributes follows.
### `request` Element ###
This is the envelope around the entire request.  The request attributes describe global data describing the machine or the Omaha client instance.  The response attributes are minimal.

_Request Attributes_
  * `protocol`: Always `"3.0"`.
  * `version`: The version number of the Omaha client.
  * `ismachine`: `0` or `1`, depending on whether the Omaha client is a per-user or per-machine instance.
  * `requestid`: A random GUID generated for each request.  (This is used to identify duplicate requests that are sent multiple times by buggy network stacks and/or proxy servers.)
  * `sessionid`: A random GUID associated with the current task - a clean install, a update check, etc.  If a task requires sending multiple requests from different processes (for example, an Omaha self-update that sends event pings both before and afterwards), those requests will have identical session IDs.
  * `userid` (optional): A random GUID associated with the logged-in user.  There will be a different userid for each username on the machine, as well as for the per-machine instance.  UIDs are opt-in and will not be generated if not opted into.
  * `installsource` (optional): An arbitrary string that can mark the source of a request.  This is set by the client driving Omaha.  Typical examples: `"scheduledtask"`, `"ondemandupdate"`, `"selfupdate"`, `"update3web"`, `"oneclick"`, and so on.
  * `originurl` (optional): If Omaha is invoked via a web browser plugin, contains the URL of the page invoking it.
  * `testsource` (optional): Identifies requests related to development and testing.  May be omitted, or one of `"dev"`, `"qa"`, `"prober"`, `"auto"`, `"ossdev"`.
  * `dedup`: Identifies the algorithm used for reporting application activity. It may be either `"cr"` or `"uid"`, for client regulated and unique user id respectively. In its recent versions, Omaha is always using `"cr"`. Please see the description of attributes for the `"ping"` element in this document for how the application activity is reported.
  * `updaterchannel`: Identifies the channel that the updater is running on; for example `"dev"`, `"stable"`, `"beta"`, or `"canary"`.

_Response Attributes_
  * `protocol`: Always `"3.0"`.
  * `server`: A string identifying the server that provided the response, mostly for diagnostic purposes.  Typical examples: `"prod"`, `"unittest"`, etc.

### `hw` Element ###
Inside the request envelope is an `hw` element that describes the hardware features of the machine. The server may not supply an `hw` element in the response.

_Request Attributes_
  * `sse`: "1" if the machine's hardware supports the SSE instruction set. "0" if the machine's hardware does not support SSE. "-1" if unknown. Default: "-1".
  * `sse2`: "1" if the machine's hardware supports the SSE2 instruction set. "0" if the machine's hardware does not support SSE2. "-1" if unknown. Default: "-1".
  * `sse3`: "1" if the machine's hardware supports the SSE3 instruction set. "0" if the machine's hardware does not support SSE3. "-1" if unknown. Default: "-1".
  * `ssse3`: "1" if the machine's hardware supports the SSSE3 instruction set. "0" if the machine's hardware does not support SSSE3. "-1" if unknown. Default: "-1".
  * `sse41`: "1" if the machine's hardware supports the SSE4.1 instruction set. "0" if the machine's hardware does not support SSE4.1. "-1" if unknown. Default: "-1".
  * `sse42`: "1" if the machine's hardware supports the SSE4.2 instruction set. "0" if the machine's hardware does not support SSE4.2. "-1" if unknown. Default: "-1"
  * `avx`: "1" if the machine's hardware supports the AVX instruction set. "0" if the machine's hardware does not support AVX. "-1" if unknown. Default: "-1"
  * `physmemory`: The physical memory the machine has available to it, or "-1" if unknown. This value could be slightly different than the amount of memory installed in the machine. Omaha client truncates the available physical memory to the nearest gigabyte. Default: "-1".

_Response Attributes_

N/A

### `os` Element ###
Inside the request envelope, but prior to any app-request elements, is an `os` element that describes the O/S of the machine.  The server never supplies an `os` element in the response.

_Request Attributes_
  * `platform`: One of `"win"`, `"mac"`, `"ios"`.
  * `version`: On Windows, the major-minor O/S version, e.g. `"5.1"`, on the Mac, it is `"MacOSX"`. On iOS, `"4.3"`, `"5.1.1"`, etc.
  * `sp`: On Windows, the service-pack version, e.g. `"Service Pack 2"`, on the Mac, OS version and processor architecture, like `"10.5.6_i486"` or `"10.4.11_ppc750"`.
  * `arch`: One of `"x86"`, `"x64"`, `"arm"`.

_Response Attributes_

N/A

### `daystart` Element ###
Inside the request envelope, but prior to any app-response elements, is a `daystart` element that gives the number of seconds since midnight at the server's locale.  This is used by the client to calculate days-since attributes in pings.

_Request Attributes_

N/A

_Response Attributes_
  * `elapsed_second`s: Number of seconds since midnight at the server's locale.

### `app` Element ###
The `app` element may be repeated in the request any number of times.  The server will reply with a corresponding app element for each request app element; In practice, the `app` elements of the response will be in the same order as the request.

_Request Attributes_
  * `appid`: The id of the app.  On Windows this is a GUID, including the braces ({}); on Mac it is a bundle ID of the form _com.google.appname_.  You can use GUIDs on the Mac, but it's usually easier to just get the application's bundle ID.
  * `version`: The dotted-quad version of the app.  A `""` value is interpreted as `"0.0.0.0"`, namely a new install.
  * `nextversion`: The dotted-quad version of the app upgrade in progress.  This is mainly used for sending events during or after an upgrade; it will be `""` for initial update checks.
  * `lang`: Language of the app install. Should be in BCP 47 representation.
  * `brand`: Brand code specified at install time, if any. These are typically used to tabulate partner deals and website promotions.
  * `client`: Similar to brand code.
  * `ap` (optional): Additional parameters specified at install time, if any. These are usually used to denote experimental branches/tracks.
  * `experiments` (optional): An experiment key/value list, if any.  This can be specified at install time, or set by the server in previous update responses. These are usually used to denote experimental branches/tracks.
  * `iid` (optional): A random GUID used to uniquely count install attempts. For example, if a user fails to install then re-runs the installer and succeeds, we might want to count that as one "attempt".
  * `installage` (optional): The number of days since the app was first installed.  A new install should use -1 days.
  * `cohort` (optional): A machine-readable string identifying the release cohort (channel) that the app belongs to. Limited to ASCII characters 32 to 127 (inclusive) and a maximum length of 1024 characters.
  * `cohorthint` (optional): An machine-readable enum indicating that the client has a desire to switch to a different release cohort. The exact legal values are app-specific and should be shared between the server and app implementations. Limited to ASCII characters 32 to 127 (inclusive) and a maximum length of 1024 characters.
  * `cohortname` (optional): A stable machine-readable enum indicating which (if any) set of messages the app should display to the user. For example, an app with a cohortname of "beta" might display beta-specific branding to the user. Limited to ASCII characters 32 to 127 (inclusive) and a maximum length of 1024 characters.

_Response Attributes_
  * `appid`: The id of the app. See Request Attributes.
  * `status`: One of:
    * `"ok"`: The appid was recognized; see the status attribute on the child `updatecheck` or `event` tags for further status.
    * `"restricted"`: The appid was recognized, but the application is not available to this user (usually based on country export restrictions).
    * `"error-unknownApplication"`: The appid was not recognized and no action elements are included.
    * `"error-invalidAppId"`: The appid is not properly formed; no action elements are included.
    * ...any other string prefixed by `"error-"`: Added to the protocol as needed.
  * `experiments` (optional): An experiment key/value list, if any.  If the server responds with any entries in this attribute, the client should save those experiments in the Registry or other store, and include them in later requests.
  * `cohort` (optional): See cohort in the request. If this attribute is transmitted in the response (even if the value is empty-string), the client should overwrite the current cohort of this app with the sent value. Limited to ASCII characters 32 to 127 (inclusive) and a maximum length of 1024 characters.
  * `cohorthint` (optional): See cohorthint in the request. If sent (even if the value is empty-string), the client should overwrite the current cohorthint of this app with the sent value. Limited to ASCII characters 32 to 127 (inclusive) and a maximum length of 1024 characters.
  * `cohortname` (optional): See cohortname in the request. If sent (even if the value is empty-string), the client should overwrite the current cohortname of this app with the sent value. Limited to ASCII characters 32 to 127 (inclusive) and a maximum length of 1024 characters.

### App Action Elements ###
Actions are always the child of an app element and pertain to that app.

#### `updatecheck` Element ####
Request whether a newer version of the application is available.

_Request Attributes_
  * `tttoken` (optional): An access token for protected downloads.  The request should be sent over SSL if this attribute is present.
  * `updatedisabled` (optional): `"true"` indicates that an update response will not be applied because it is disallowed by Group Policy.
  * `targetversionprefix` (optional): A prefix of the update version that the client is ok receiving. If ended with $ then the version string must match exactly. (It may be ignored by the server.)

_Response Attributes_

The `"status"` attribute is always present; the rest are supplied only if an update is available.

The update check is over a secure channel.  By providing the size and hash of the download binary, the binary itself may be fetched over an insecure channel and still be verified.

  * `status`: One of:
    * `"noupdate"`: No update is available.
    * `"ok"`: An update specification is included.
    * `"error-osnotsupported"`: The operating system version is not supported by this application.
    * `"error-unsupportedProtocol"`: The update specification cannot be expressed in this version of the protocol.
    * `"error-pluginRestrictedHost"`: This application is restricted to only be installable from specific domains.
    * `"error-hash"`: An internal error occurred on the server while obtaining the hash of the installer binary.
    * `"error-internal"`: Other internal error.
  * `tttoken` (optional): An access token for protected downloads.  The request should be sent over SSL if this attribute is present.
  * `errorurl` (optional): A URL that provides more information for the error in status. The Omaha client supports this for "error-osnotsupported".

On a status value of `"ok"`, additional children will be included.
#### `event` Element ####
Notifies the server of the progress or result of an install/update in progress.

_Request Attributes_
  * `eventtype`: One of
    * 0: unknown
    * 1: download complete
    * 2: install complete
    * 3: update complete
    * 4: uninstall
    * 5: download started
    * 6: install started
    * 9: new application install started
    * 10: setup started
    * 11: setup finished
    * 12: update application started
    * 13: update download started
    * 14: update download finished
    * 15: update installer started
    * 16: setup update begin
    * 17: setup update complete
    * 20: register product complete
    * 30: OEM install first check
    * 40: app-specific command started
    * 41: app-specific command ended
    * 100: setup failure
    * 102: COM server failure
    * 103: setup update failure
  * `eventresult`: One of
    * 0: error
    * 1: success
    * 2: success reboot
    * 3: success restart browser
    * 4: cancelled
    * 5: error installer MSI
    * 6: error installer other
    * 7: noupdate
    * 8: error installer system
    * 9: update deferred
    * 10: handoff error
  * `errorcode`: (optional)
    * For `eventresult`==0: Omaha error code
    * For `eventresult`=={1 | 2 | 3| 5 | 6 | 8}: Application-specific installer exit/result code.
  * `extracode1`: (optional) Additional numerical information related to errorcode.
  * `download_time_ms`: (optional) time taken for the download (if there had been one, note for cached app, download time is 0)
  * `downloaded`: (optional) bytes downloaded
  * `total`:(optional) sum of all packages sizes in this app
  * `update_check_time_ms`: (optional) time taken to do an update check for the app.
  * `install_time_ms`: (optional) time take to install Omaha or an app.
  * `source_url_index`: (optional) Index of the URL that served app installer download.
  * `state_cancelled`: (optional) App state when user cancels installation. One of:
    * 0: unknown
    * 1: init
    * 2: waiting to check for update
    * 3: checking for update
    * 4: update available
    * 5: waiting to download
    * 6: retrying download
    * 7: downloading
    * 8: download complete
    * 9: extracting
    * 10: applying differential patch
    * 11: ready to install
    * 12: waiting to install
    * 13: installing
    * 14: install complete
    * 15: paused
    * 16: no update
    * 17: error
  * `time_since_update_available_ms`: (optional) Time interval between update is available and user cancels installation.
  * `time_since_download_start_ms`: (optional) Time interval between update download starts and user cancels installation.
  * `nextversion`: (optional) The version of the app that an update was attempted to (regardless of whether or not the update succeeded).
  * `previousversion`: (optional) The version of the app prior to the update attempt.

_Response Attributes_
  * `status`: Always `"ok"`.

#### `ping` Element ####
Reports the activity level of this application since the previous update check.

_Request Attributes_

  * `active`: "1" if the app was active since the last report; "0" otherwise.
  * `a`: If present, the app was active since the last report; the value is the number of days since the app last reported active.  The attr is omitted if the app was not active.
  * `r`: The app is present; the value is the number of the days since the app last reported present.

For both `a` and `r`, the special value "-1" indicates a new install.  Any attribute may be omitted if its value is "0"; the entire element may be omitted if all values are zero.

If `a` is nonzero, `active` should be set to "1".

_Response Attributes_
  * `status`: Always `"ok"`.

#### `data` Element ####
This element provides a way to pass data to the application installer.  There can be many data elements in a request. In general, this data represents application configuration or some kind of run time parameter that the application installer needs, for a particular installation context. This data is less _trusted_ in the sense that it is usually provided by 3rd parties and it is more vulnerable to attacks, such as MITM. The application code must take steps to harden and sanitize the data before use.

Currently, two mechanisms are provided here: `install` and `untrusted`.

In the case of `install`, the metainstaller or install command line includes an insecure data `index`, which corresponds to a specific blob of install data on the server.  The client securely requests the data for this index from the server using the `data` element.

Additional arbitrary data can be passed by using `untrusted`. The source for the `untrusted` is usually a metainstaller tag, which is subsequently passed to the Google Update run time as an extra argument of the command line.  Upon receiving the `untrusted` in the request, the  server is expected do to additional validation. The server will reply with a status of `"error-invalidargs"` if the `untrusted` is invalid for any reason.

_Request Attributes_
  * `name`: A collection specifier (must be alphanumeric;  `"install"` and `"untrusted"` are currently implemented).
  * `index`: For `"install"`, the requested element of the collection (must be alphanumeric).

For `"untrusted"`, the data itself is sent as a text element of the data element.

_Response Attributes_
  * `status`: One of:
    * `"ok"`: Data blob included.
    * `"error-invalidargs"`: the name, index, or untrusteddata were not alphanumeric or were invalid for any reason.
    * `"error-nodata"`: the named collection or index was not found.
  * `name`: The collection specifier; could be `"install"` or `"untrusted"` respectively.
  * `index`: For `"install"`, the requested element of the collection.

For `"install"`, the data itself is returned as a text element child of the data element. There is no data returned for `"untrusted"`.

#### `unknown` Element ####
If the client sends an unrecognized child element of the app element, the server may respond with an `"unknown"` element in the corresponding position of the response.  It will always have a `status` attribute with a value of `"error"`.  (This is meant to maintain the structure of the response, which mirrors the app and action elements of the request.)

### UpdateCheck Response Elements ###
When an `updatecheck` response element has a status of "ok", the following child elements are included, describing the method for performing the update check.

#### `urls` Element ####
Describes a set of locations where installer binaries can be downloaded.

Contains no attributes.  Contains one or more url child elements describing URLs prefixes where a package can be downloaded.

##### `url` Element #####
Contains a URL fragment where packages in the following `manifest` element can be found.  A complete URL to each installer binary may be formed by appending the filename in a `package` element to this URL fragment.

The URL fragment is stored as a text child element.

#### `manifest` Element ####
Describes the files needed to install the new version of an application, and the actions needed to be taken with those files.

_Request Attributes_
> N/A

_Response Attributes_
  * `version` (optional): Contains the version that this manifest should deliver when finished.

The `manifest` element should contain two child elements - one `packages` element and one `actions` element.

#### `packages` Element ####
Describes the set of files needed to install the application.

Contains no attributes.  Contains one or more `package` child elements.

##### `package` Element #####
Describes a single file needed to install the application.

_Request Attributes_
> N/A

_Response Attributes_
  * `name`: Contains a filename to download.  Omaha will sequentially attempt to build a URL by appending this name to each url in the `urls` element and download it.
  * `required`: Contains `"true"` or `"false"`.  Denotes whether or not the file is required for the install to succeed.
  * `size`: Contains the size in bytes of the installer binary.
  * `hash`: Contains the SHA-1 hash of the installer binary, encoded in base64.

#### `actions` Element ####
Describes the actions to perform to install the application once all required files in the `packages` element have been successfully downloaded.

Contains no attributes.  Contains one or more `action` child elements.

##### `action` Element #####
Describes a single action to perform as part of the install process.

_Request Attributes_
> N/A

_Response Attributes_
  * `event`: Contains a fixed string denoting when this action should be run.  One of:
    * `"preinstall"`
    * `"install"`
    * `"postinstall"`
    * `"update"`
  * `run` (optional): The name of an installer binary to run.  (Typically, this binary was specified to be downloaded in the prior `packages` element.)
  * `arguments` (optional): Arguments to be passed to that installer binary.
  * `successurl` (optional): A URL to be opened using the system's default web browser on a successful install.
  * `terminateallbrowsers` (optional): If `"true"`, close all browser windows before starting the installer binary.
  * `successsaction` (optional): Contains a fixed string denoting some action to take in response to a successful install.  One of:
    * `"default"`
    * `"exitsilently"`
    * `"exitsilentlyonlaunchcmd`"

## Advisory HTTP Headers ##
The Windows Client currently attaches some custom HTTP headers to the request; they are mostly provided to aid the server in statistics generation.  The client may send none, some, or all of them as appropriate for the current request.

### Network Diagnostics Headers ###

If any of these headers are present, they imply that the network request was unsuccessful in the past and has been retried at least once.
  * `X-Retry-Count` -- Contains the total number of times that this network request has been tried (not counting proxy or protocol fallbacks).
  * `X-HTTP-Attempts` -- Contains the total number of times that this network request has been tried (counting proxy and protocol fallbacks).
  * `X-Last-HR` -- Contains the HRESULT error code returned from `NetworkRequestImpl::DoSendHttpRequest()` for the previous attempt.
  * `X-Last-HTTP-Status-Code` -- Contains the HTTP status code, if any, that we received for the previous attempt.
  * `X-Proxy-Retry-Count` -- If this header is present, it means that the request is being sent via at least one proxy that required authentication.  Contains the number of times that we've received a HTTP 407 (Proxy Authentication Required) status code in the process of sending this request.
  * `X-Proxy-Manual-Auth` -- If this header is present, it means that this request is being sent through at least one proxy that required authentication **and** that Omaha had no cached credentials and displayed a UI to prompt the user.  The header's value is always `1`.

### Persisted Ping Headers ###

There may be cases where Omaha completes an update/install, but the network connection fails during the install, and the event ping notifying the server of the success/failure cannot be sent.  (This is particularly common on laptops with wireless connections.)  In this case, the client will serialize the event ping to the Registry, and attempt to resend it at a later date when the network connection is available again.

  * `X-RequestAge` -- If this header is present, it means that this request is an event ping that was persisted and is now being re-sent.  The value is the age in seconds between the current time and the time at which the ping request was originally attempted.

## Security Headers ##
The response must be secure: the client must be sure the response data blob came from Google and that it was not tampered with.  This is done by either
  * using SSL
  * using the secure Client-Update Protocol (CUP)

If the client elects to use SSL, no further integrity checking is needed.  If CUP is used, the following HTTP headers and parameters will be added, which allow a signature to be supplied by the server and verified by the client.  The [CUP design document](http://omaha.googlecode.com/svn/wiki/cup.html) describes the protocol in detail.

## Other Headers ##
  * `X-GoogleUpdate-Interactivity` -- Either 'fg' or 'bg' when present, 'fg' indicating a user-initiated foreground update. 'bg' indicates that the request is part of a background update. If the server is under extremely high load, it may use this to prioritize 'fg' requests over 'bg' requests.

### CUP Request ###
  * `w` (URL parameter): Encodes a proposed private key.  Provides nonce for protection against replay in the signed response.
  * `If-Match` (HTTP header): Signature that proves the client knows its own private key.
  * `Cookie` (HTTP header): (optional)  Encrypted copy of the client's private verification key. (Not related to any browser cookie.)

### CUP Response ###
  * `ETag` (HTTP header): Response signature
  * `Set-Cookie` (HTTP header): (optional)  Encrypted copy of the client's private key, for the client to send in the next request. (Not related to any browser cookie.)

### Implementation details ###
The server is expected to return a 400 response when the body of the request is empty. The body of the response will contain the reason for the the 'bad request', for instance, the server could not parse the request.