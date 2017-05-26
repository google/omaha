# Introduction
This document is a markdown representation of the original Client Update Protocol design document. See also the [ECDSA extension](ClientUpdateProtocolEcdsa.md) to the protocol.

# Objective
Omaha and various other client products ping our servers for updates on a periodic basis. SSL provides the freshness and authenticity needed but comes with substantial overhead, both on the server- and client-side. We have empirical evidence that some clients are able to ping for updates over HTTP, but fail to do so over HTTPS. For embedded devices, requiring a SSL stack just to perform secure updates might not be feasible.

Client Update Protocol goals are:
* Provide more efficient yet secure alternative to SSL for update checks and similar client-server requests.
* Use HTTP transport layer, including traversing proxies.
* Have a small, self-contained implementation suitable for resource constrained deployments.

Non-goals:
* Replace SSL everywhere.
* Provide privacy of the communication. 
* Authenticate clients / DRM.

# Background
To securely check for and download updates we need to protect the communication. The protection needs to cover:
* Authenticity. An attacker should not be able to replace or modify message content on the wire.
* Freshness. An attacker should not be able to trick a client into upgrading to an authentic but stale and vulnerable version.

# Overview
Observations leading to a alternate design are:
* For fresh, authenticated update checks we do not need all of the SSL features:
    * The PKI part is not needed since we control both the client and the server and need not confer trust with anyone else.
    * Privacy of the download data is not needed since we do not intend to deliver private content; the downloads are free for all.
    * Integrity of requests and message meta-data (headers etc) might not be needed as long as the meta-data we act on is authentic.
    * We only need a few of the cryptographic primitives and need not negotiate.
    * Request replay protection (request freshness from the server perspective) is not essential for update checks and similar idem-potent interactions.
* We control all of the client code and communication stack, unlike the typical web-application scenario involving browsers.
* For thin clients, the SSL requirement comes with a lot of code bloat.
* We can achieve better handshake amortization than non-rfc5077 SSL stacks offer.
* When relaxing request replay protection, we can achieve reply authenticity in a single round-trip.

# Detailed Design
There are two distinct parts that need to be designed:
* A protocol. 
* How to combine the protocol with the HTTP transport layer.

## Protocol
The client knows a public key pk[v], and might have opaque cookie c and associated shared key sk.

The server has private keys priv[] and has server secret keys ss[].


### Cryptographic Primitives
* SYMsign~key~(data) := HMAC-SHA1
* HASH(data) := SHA1
* RSApad~|keybits|~(data) := (r | SHA1(r))[0..keybits-1]; msb (keybits-1-160) bits random; lsb 160 bits SHA1 of the random bits.
* RSAencrypt~key~(data) := Raw RSA encrypt; Public exponent 3.
* RSAdecrypt~key[]~(data) := Raw RSA decrypt.
* RSAcheckpad(data) := Verify lsb 160 bits are SHA1 of remaining bits.
* SYMencrypt~key~(data) := key version | HMAC | AES-CTR, using HMAC as IV.
* SYMdecrypt~key[]~(data) := key version | HMAC | AES-CTR, using HMAC as IV.

### (1) Client
```
r := RSApad~|pk[v]|~(entropy)
sk' := HASH(r)
w := RSAencrypt~pk[v]~(r)
ƕw := HASH(HASH(v|w)|HASH(req)|HASH(body)?)
Send v|w
If client has c, sk
  cp := SYMsign~sk~(0|ƕw|HASH(c))
  Send c
else
  cp := SYMsign~sk'~(3|ƕw)
Send cp
```
### (2) Client → Server Message
```req, v|w, cp, [c], [body]```

### (3) Server
```
ƕw := HASH(HASH(v|w)|HASH(req)|HASH(body)?)

If request contains c
  cv,cd := parse c
  sk := SYMdecrypt~ss[cv]~(cd)

If (sk && cp == SYMsign~sk~(0|ƕw|HASH(c)))
  rsp := generate response for req
  ƕm := HASH(rsp)
  sp := SYMsign~sk~(2|ƕw|ƕm)
  Send sp, rsp
else
  r := RSAdecrypt~priv[v]~(w)
  if (!RSAcheckpad(r))
    LOG "Client does not know how to RSAencrypt!"
  sk' := HASH(r)
  If (cp != SYMsign~sk'~(3|ƕw)
    LOG "Client does not know how to SYMsign!"
  csv := current server cookie encryption key version
  c' := csv|SYMencrypt~ss[csv]~(sk')
  rsp := generate response for req
  ƕm := HASH(rsp)
  sp := SYMsign~sk'~(1|ƕw|ƕm|HASH(c'))
  Send sp, c', rsp
```

### (4) Server → Client Message
```sp, [c'], rsp```

### (5) Client
```
ƕm := HASH(rsp)

If (c' &&  sp == SYMsign~sk'~(1|ƕw|ƕm|HASH(c')))
  Store sk := sk', c := c'
else
If (sp != SYMsign~sk~(2|ƕw|ƕm))
  FAIL
```

### Observations
Freshness is achieved by having the client pick a fresh w for every request.

The client operations are efficient enough to always both RSAencrypt and SYMsign. The client accepts authenticated responses for either sk or sk'.

We can amortize the cost of RSAdecrypt on the server-side over many requests. Either the client or the server can initiate roll-over to a fresh shared secret by not sending or not honoring the cookie. Typically our client will always send the cookie and our server manages the length of the exposure. 

An attacker could replay a request to the server and would get a valid response; this could pollute server statistics. Care must be taken to make sure our request handling remains essentially idem-potent. 
If idem-potency is not feasible, a server-side challenge, synchronous time or server-side bookkeeping can be used to make requests be one-time events. For the envisioned update protocols this is not required. Note this is due to the protocol not authenticating the client in any shape or form.     

RSApad / RSAcheckpad are not cryptographically required (compared to using |keybits| of random) but do provide a signal whether clients have the public key they claim they have and/or whether the client library is able to RSAencrypt properly.

cp (client proof) is used to tell whether the client knows the sk associated with cookie c. Having the server test this allows for more graceful fail over to a fresh sk in case the client state somehow got out of sync.

If c nor w decrypts correctly the server could return a specific response which triggers a fall-back to a SSL update protocol.

The server need not use the same ss[x] for every c' it hands out. And different servers could use different ss[x] as long as all servers in the server pool know all ss[] in circulation. Note there is no real security benefit to doing this.

An attacker could try to DoS the update servers by triggering the expensive RSAdecrypt for many requests in parallel. When under attack, the servers could throttle or reject requests that do not offer valid c and cp. Note validating c and cp requires knowledge of ss[].

Whenever a cookie (c or c') is sent over the wire, the sender includes it in the cryptographic proof along with it (cp or sp). An intermediary stuffing or removing a cookie on the wire will not achieve anything beyond failing of the proofs. The client will not store a cookie that the server did not intend to be associated with the shared secret.

The hash of the request (req) is carried through the cryptographic proofs. Results from requests for resource A cannot be mistaken for results for requests for resource B.

RSAdecrypt takes about 3 msec on a single 32-bit core. Less than half that on a 64-bit core in 64-bit mode. All other primitives (SYMdecrypt, SYMsign) take orders of magnitude less time. RSAencrypt happens on the client but is a lot less computationally intensive to start with. Probably less than 0.1 msec.

## HTTP Transport Mapping
We propose the following mapping of the protocol message parts to the HTTP transport. All values are to be WebSafeBase64Encoded.
* w is URL parameter (e.g. /update?w=hhrk2hkjh23r2rkjhfdkjhas)
* c is sent as a regular cookie (e.g. Cookie: c=kjdlkajdla; Set-Cookie: c=kjasldkjalksdj)
* cp is sent as If-Match field (e.g. If-Match: "jksdflsjfl2h23t2")
* p is sent as ETag field (e.g. ETag: "lkajsdlkajflkjg2iljt2tlkjsdflj12341")
* content gets sent using the regular Content-Length header and HTTP message body.

Additional HTTP request / response headers (Connection: Keep-Alive, ..) are allowed but not relevant.

### Observations
Sending w as URL parameter defeats all caching nicely. Server responses should still carry no-cache headers to relieve pressure on caches but at least mis-configured caching is less likely to cause trouble.

Sending c as Cookie makes it clear to the observer this value identifies the client / session, as normal HTTP cookies do. No expiration or domain attributes are relevant for CUP.

If-Match contains a check value which the server uses to verify whether the client actually knows the key which is embedded in the cookie.

ETag uniquely identifies the content for given w and shared key. Not alien to its envisioned use.

Other mappings are possible and possibly desirable if overloading semantics of existing HTTP headers is deemed to risky / confusing.

We believe the Cookie/Set-Cookie and URL parameter path choices are at low risk of getting disturbed at the HTTP transport layer; If-Match and ETag are believed to be propagated as expected as well.

Since we are in effect hashing the content, any bit-level tampering / altering of the content during transport will make the checks fail.

Content-Type: application/octet-stream or some Content-Encoding headers might be needed to stop meddling proxies from tampering and hence invalidating content.

A User-Agent string indicating the CUP library version might be present as well; the public key version as encoded in w is most likely already unique to a CUP library version.

### Code Location
Omaha's implementation of this protocol is consists of cup_request module, available here. From a design perspective, cup_request features a decorator design pattern, that allows CUP capabilities to be added to a simple HTTP request.
