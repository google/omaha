# Omaha Client-Server Protocol V2 #

This document describes version 2 of the Omaha client-server protocol.  Omaha was launched on Windows with this version of the protocol in May 2007.  (Version 1 of the protocol was never deployed publicly.)  Google Software Update launched on Mac with this version in May 2008.

## Overview ##
The client sends requests via HTTP POST with an XML data body.  The response is an XML data body.

![http://omaha.googlecode.com/svn/wiki/ServerProtocol_Images/omahaprotocol2.jpg](http://omaha.googlecode.com/svn/wiki/ServerProtocol_Images/omahaprotocol2.jpg)

An HTTP request may concatenate multiple applications in one XML body; similarly, multiple request-actions may be included for any application.  The server responds with a status and other information (as appropriate) for each action, organized in a similar nested XML structure.


## Examples ##
### Update Check ###
This example also shows bundling of requests for two different applications.

_Request:_

```
<?xml version="1.0" encoding="UTF-8"?>
<o:gupdate xmlns:o="http://www.google.com/update2/request" protocol="2.0" version="1.2.183.7" ismachine="0"
     machineid="{B421053D-AA39-418A-B6C3-123456789ABC}" userid="{422ED4F2-699D-49E1-9D60-123456789ABC}"
     requestid="{D7E8D72F-C657-4119-AA48-123456789ABC}">
  <o:os platform="win" version="5.1" sp="Service Pack 2"/>
  <o:app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" version="1.2.183.7" lang="en" brand="GGLS"
      client="" installage="32" installsource="scheduler">
    <o:updatecheck tag="beta"/>
  </o:app>
  <o:app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" version="2.0.172.37" lang="en" brand="GGLS"
      client="" installsource="scheduler">
    <o:updatecheck/>
    <o:ping active="1"/>
  </o:app>
</o:gupdate> 
```

_Response (negative):_

There is not an update for either of the apps. The app and ping elements are acknowledged with status="ok".
```
<?xml version="1.0" encoding="UTF-8"?>
<gupdate xmlns="http://www.google.com/update2/response" protocol="2.0">
  <daystart elapsed_seconds="59892"/>
  <app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" status="ok">
    <updatecheck status="noupdate"/>
  </app>
  <app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" status="ok">
    <updatecheck status="noupdate"/>
    <ping status="ok"/>
  </app>
</gupdate> 
```

_Response (positive):_

There is an update available for only the second app. The app and ping elements are acknowledged with status="ok".
```
<?xml version="1.0" encoding="UTF-8"?>
<gupdate xmlns="http://www.google.com/update2/response" protocol="2.0">
  <daystart elapsed_seconds="59892"/>
  <app appid="{430FD4D0-B729-4F61-AA34-91526481799D}" status="ok">
    <updatecheck status="noupdate"/>
  </app>
  <app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" status="ok">
    <updatecheck Version="2.0.172.37" arguments="--do-not-launch-chrome"
        codebase="http://cache.pack.google.com/edgedl/chrome/install/172.37/chrome_installer.exe"
        hash="NT/6ilbSjWgbVqHZ0rT1vTg1coE=" needsadmin="false" onsuccess="exitsilentlyonlaunchcmd"
        size="9614320" status="ok"/>
    <ping status="ok"/>
  </app>
</gupdate> 
```

### Event Report ###
Events are reports from the client to the server; no server response data are required, but they should be acknowledged with status="ok".

_Request:_
```
<?xml version="1.0" encoding="UTF-8"?>
<o:gupdate xmlns:o="http://www.google.com/update2/request" protocol="2.0" version="1.2.183.7" ismachine="0"
    machineid="{B421053D-AA39-418A-B6C3-123456789ABC}" userid="{422ED4F2-699D-49E1-9D60-123456789ABC}"
    requestid="{9F6D6EC4-D6DE-4ADF-8E10-123456789ABC}">
  <o:os platform="win" version="5.1" sp="Service Pack 2"/>
  <o:app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" version="" lang="en" brand="" client=""
      iid="{9F2E46A2-01B5-8073-4FBB-123456789ABC}">
    <o:event eventtype="1" eventresult="1" errorcode="0" extracode1="0" previousversion="2.0.172.37"/>
  </o:app>
</o:gupdate> 
```

_Response:_
```
<?xml version="1.0" encoding="UTF-8"?>
<gupdate xmlns="http://www.google.com/update2/response" protocol="2.0">
  <daystart elapsed_seconds="59892"/>
  <app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" status="ok">
    <event status="ok"/>
  </app>
</gupdate> 
```

### Data Request ###
Install data are requested at install time; the updatecheck in this example has been removed for clarity.

_Request:_
```
<?xml version="1.0" encoding="UTF-8"?>
<o:gupdate xmlns:o="http://www.google.com/update2/request" protocol="2.0" version="1.2.183.7" ismachine="0"
    machineid="{B421053D-AA39-418A-B6C3-123456789ABC}" userid="{422ED4F2-699D-49E1-9D60-123456789ABC}"
    requestid="{204A2EC0-9CB7-4A7C-AEAA-123456789ABC}">
  <o:os platform="win" version="5.1" sp="Service Pack 2"/>
  <o:app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" version="" lang="en" brand="" client=""
      iid="{9F2E46A2-01B5-8073-4FBB-123456789ABC}">
    <o:data name="install" index="custominstallconfigfoo"/>
  </o:app>
</o:gupdate> 
```

_Response:_
```
<?xml version="1.0" encoding="UTF-8"?>
<gupdate xmlns="http://www.google.com/update2/response" protocol="2.0">
  <daystart elapsed_seconds="59892"/>
  <app appid="{8A69D345-D564-463C-AFF1-A69D9E530F96}" status="ok">
    <data index="custominstallconfigfoo" name="install" status="ok">app-specific values here
    </data>
  </app>
</gupdate> 
```

## Reference ##
A terse description of the XML elements and attributes follows.
### gupdate Element ###
This is the envelope around the entire request.  The request attributes describe global data describing the machine or the Omaha client instance.  The response attributes are minimal.

_Request Attributes_
  * xmlns: Always "http://www.google.com/update2/request".
  * protocol: Always "2.0".
  * version: The version number of the Omaha client.
  * ismachine: 0 or 1, depending on whether the Omaha client is a per-user or per-machine instance.
  * machineid (optional): A random GUID associated with the machine.  All users on a machine will have the same machineid.
  * userid (optional): A random GUID associated with the logged-in user.  There will be a different userid for each username on the machine, as well as for the per-machine instance.
  * testsource (optional): Identifies requests related to development and testing.  May be omitted, or one of "dev", "qa", "prober", "auto".
  * requestid (optional): A random GUID uniquely identifying this request. Could be used to de-duplicate multiple requests or retries (i.e. through proxies).

_Response Attributes_
  * xmlns: Always "http://www.google.com/update2/response".
  * protocol: Always "2.0".

### os Element ###
Inside the gupdate envelope, but prior to any app-request elements, is an os element that describes the O/S of the machine.  The server never supplies an os element in the response.

_Request Attributes_
  * platform: One of "win", "mac".
  * version: On Windows, the major-minor O/S version, e.g. "5.1", on the Mac, it is "MacOSX".
  * sp: On Windows, the service-pack version, e.g. "Service Pack 2", on the Mac, OS version and processor architecture, like "10.5.6\_i486" or "10.4.11\_ppc750".

_Response Attributes_

N/A

### daystart Element ###
(Added February 2010)  Inside the gupdate envelope, but prior to any app-response elements, is a daystart element that gives the number of seconds since midnight at the server's locale.  This is used by the client to calculate days-since attributes in pings.

_Request Attributes_

N/A

_Response Attributes_
  * elapsed\_seconds: Number of seconds since midnight at the server's locale.

### app Element ###
The app element may be repeated in the request any number of times.  The server will reply with a corresponding app element for each request app element; In practice, the app elements of the response will be in the same order as the request.

_Request Attributes_
  * appid: The id of the app.  On Windows this is a GUID, including the braces ({}); on Mac it is a bundle ID of the form com.google.appname.  You can use GUIDs on the Mac, but it's usually easier to just get the application's bundle ID.
  * version: The dotted-quad version of the app.  A "" value is interpreted as "0.0.0.0", namely a new install.
  * lang (optional): Language of the app install. Should be in BCP 47 representation.
  * brand (optional): Brand code specified at install time, if any. These are typically used to tabulate partner deals and website promotions.
  * client (optional): Similar to brand code.
  * iid (optional): A random GUID used to uniquely count install attempts. For example, if a user fails to install then re-runs the installer and succeeds, we might want to count that as one "attempt".
  * installage (optional): The number of days since the app was first installed.
  * installsource (optional): Specifies the source of the request. Examples include "oneclick", "clickonce", "ondemandupdate", "ondemandcheckforupdate", "offline", "scheduler", "core". This value is specified to the Omaha client on the command line.
  * fp (optional): Specifies a version-agnostic identifier for the last downloaded binary for this app. Usually "1.X" where X is the sha-256 hash of the downloaded binary.

_Response Attributes_
  * appid: The id of the app. See Request Attributes.
  * status: One of:
    * "ok": The appid was recognized and action elements are included in the response.
    * "error-unknownApplication": The appid was not recognized and no action elements are included.
    * "error-invalidAppId": The appid is not properly formed; no action elements are included.
    * "error-restricted": The application is not available to this user (usually based on country export restrictions).
    * ...any other string prefixed by "error-": Added to the protocol as needed.

### App Action Elements ###
Actions are always the child of an app element and pertain to that app.

#### updatecheck ####
Request whether a newer version of the application is available.

_Request Attributes_
  * tag (optional): Additional app-defined data that the server can use to select a response. For historical reasons, this is called "ap" in the Omaha client.
  * tttoken (optional): An access token for protected downloads.  The request should be sent over SSL if this attribute is present.
  * updatedisabled (optional): `"true"` indicates that an update response will not be applied because it is disallowed by Group Policy.

_Response Attributes_

The "status" attribute is always present; the rest are supplied only if an update is available.

The update check is over a secure channel.  By providing the size and hash of the download binary, the binary itself may be fetched over an insecure channel and still be verified.

  * status: One of:
    * "noupdate": No update is available.
    * "ok": An update specification is included.
    * "error-hash": Internal error in obtaining the hash of the installer binary.
    * "error-osnotsupported": The operating system version is not supported.
    * "error-internal": Other internal error.
  * codebase: URL of the installer binary.
  * hash: SHA-1 hash of the installer binary, encoded in base64.
  * size: Size in bytes of the installer binary.
  * needsadmin: "true" or "false", depending on whether it should be a machine or per-user install Currently ignored. The needsadmin value embedded in the initial Omaha download is the one that matters.
  * arguments: Additional arguments to pass to the installer binary on its command line.
  * errorurl (optional): A URL that provides more information for the error in status. The Omaha client supports this for "error-osnotsupported".

In addition, the Omaha client supports the following optional attributes:
  * onsuccess: Action to take after successful installation.
    * default: Display the success dialog.
    * exitsilently: Exit without displaying the success dialog.
    * exitsilentlyonlaunchcmd: Exit without displaying the success dialog if the command specified by the app installer was successfully launched.
  * Version: The version of the app that the app should be updated to (by running the installer binary specified by codebase).
  * successurl: URL to open upon successful installation. (Currently the Omaha client will only use this if the user chooses restart the browser(s).)
  * terminateallbrowsers: "true" or "false", specifies whether Omaha should attempt to shutdown all supported browsers or just the specified browser.
  * tttoken: If present, the client should change its tt-token to this value.  A zero-length value means the token should be deleted from local store. (The server must echo the tttoken in the request unless it wants the client to clear its token.)

And on the Mac, the Google Software Update Agent supports the following optional attributes.
  * Prompt: Whether to display the UI before installing an update
  * RequireReboot: Prompt the user for a reboot.
  * MoreInfo: An URL to load into a WebView in the Agent UI.  This can contain release notes or special instructions.  The URL can include a localization parameter.
  * LocalizationBundle: A path on disk to find localization information for the product.
  * DisplayVersion: A string to display in the Version column of the UI.

#### event ####
The event response is always status="ok", meaning the data were successfully received.

_Request Attributes_
  * eventtype: One of
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
    * 100: setup failure
    * 102: COM server failure
    * 103: setup update failure
  * eventresult: One of
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
  * errorcode: (optional)
    * For eventresult==0: Omaha error code
    * For eventresult=={1 | 2 | 3| 5 | 6 | 8}: Application-specific installer exit/result code.
  * extracode: (optional) Additional numerical information related to errorcode.
  * nextversion: (optional) The version of the app that an update was attempted to (regardless of whether or not the update succeeded).
  * previousversion: (optional) The version of the app prior to the update attempt.
  * nextfp: (optional) A short representation of the binary payload that an update was attempted to. This is a SHA-256 hash of the download, when a SHA-256 hash is available.
  * previousfp: (optional) A short representation of the binary payload of the most recent update / installation. This is a SHA-256 hash of the previous download, when a SHA-256 hash is available.

_Response Attributes_
  * status: Always "ok"

#### ping ####
The ping response is always status="ok", meaning the data were successfully received.

_Request Attributes_

(Prior to February 2010)
  * active: 0 or 1, depending on whether the application was active since the last report

(Since February 2010)

In either case, the special value "-1" indicates a new install.  An attr may be omitted if the value is "0"; and the entire ping may be omitted if both values are zero.
  * a: The app was active since the last report; the value is the number of days since the app last reported active.  The attr is omitted if the app was not active.
  * r: The app is present; the value is the number of the days since the app last reported present.

_Response Attributes_
  * status: Always "ok"

#### data ####
The metainstaller or install command line includes an insecure data "index", which corresponds to a specific blob of install data on the server.  The client securely requests the data for this index from the server using the "data" element.

_Request Attributes_
  * name: A collection specifier (must be alphanumeric)
  * index: The requested element of the collection (must be alphanumeric)

_Response Attributes_
  * status: One of:
    * "ok": Data blob included
    * "error-invalidargs": the name or index was not alphanumeric
    * "error-nodata": the named collection or index was not found
The data are returned as a text element child of the data element.

#### unknown ####
If the client sends an unrecognized child element of the app element, the server will respond with an "unknown" element in the corresponding position of the response.  The status will always be "error".  (This is meant to maintain the structure of the response, which mirrors the app and action elements of the request.)

## Security Headers ##
The response must be secure: the client must be sure the response data blob came from Google and that it was not tampered with.  This is done by either
  * using SSL
  * using the secure Client-Update Protocol (CUP)

If the client elects to use SSL, no further integrity checking is needed.  If CUP is used, the following HTTP headers and parameters will be added, which allow a signature to be supplied by the server and verified by the client.  The [CUP design document](http://omaha.googlecode.com/svn/wiki/cup.html) describes the protocol in detail.

### CUP Request ###
  * w (URL parameter): Encodes a proposed private key.  Provides nonce for protection against replay in the signed response.
  * If-Match (HTTP header): Signature that proves the client knows its own private key.
  * Cookie (HTTP header): (optional)  Encrypted copy of the client's private verification key. (Not related to any browser cookie.)

### CUP Response ###
  * ETag (HTTP header): Response signature
  * Set-Cookie (HTTP header): (optional)  Encrypted copy of the client's private key, for the client to send in the next request. (Not related to any browser cookie.)