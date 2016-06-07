# Introduction #

Starting in 2014, Omaha contains support for an alternate CUP protocol that uses ECDSA signatures for message verification instead of HMAC signatures with a RSA-encrypted key.

It can be used along side the original CUP, or as a complete replacement for it.

# Objectives #

The objective of CUP-ECDSA is similar to that of the original CUP:

**Goals:**
  * It must provide message authenticity.  MITM modification must be detected and rejected for both client requests and server responses.
  * It must provide message freshness for server responses; a MITM attacker should not be able to replay an unmodified, authentic server response from a previous transaction in response to a new client request.

**Non-goals:**
  * It does not need to provide privacy in communication.
  * It does not need to ensure freshness in client requests; as long as update checks are idempotent, we don't care if client requests are replayed.  Freshness is only crucial for server responses.
  * It does not need to authenticate the client; as long as it knows the public key and follows the protocol, the server should answer.
  * It does not need to implement PKI; for all uses of Omaha, only a single server is used at a time, and changes in the server key can be matched with upgrading the client Omaha to contain new public keys.

The main motivation in using CUP-ECDSA is reducing server load.  The original CUP's costs are dominated by the cost of RSA encrypt/decrypt operations.  Getting good server performance in CUP is critically reliant on caching common keys via HTTP cookies, allowing you to amortize the cost of RSA.

ECDSA signatures are very fast to compute, but work-intensive to verify, making them ideal for reducing load on the server.

## Key Rotation ##

The biggest danger in any system involving ECDSA is the danger of K repetition.

Computing an ECDSA signature starts with selecting a random 256-bit integer, called K.  The combination of K and the public key are used to produce the first half of the signature, called R; the values of R, K, the private key, and the message digest are used to compute the other half of the signature, called S.

Because of this process, if the same value of K is chosen for two signature, both signatures will have the same value for R.  If a malicious user can acquire two messages that have different bodies but identical R values, a straightforward computation yields the server's private key.

The chances of this happening are unlikely; the K value is a 256-bit number.  Assuming that a good PRNG is used, and properly seeded, the probability of a collision is miniscule.  However, to mitigate the potential of someone collecting enough values to find a K collision, any scheme based on ECDSA should include regular key rotations, at least once per year.

# Top-Level Description #

The server publishes an elliptic curve field/equation and a public key curve point to be used by the client.

For each request, the client assembles three components:

  * The message body (the update request to be sent to the server).
  * A small random number to be used as a client nonce for freshness (at least 32 bits).
  * A code to identify the public key the client will use to verify this request.

The client converts the public key id and nonce to a string: the public key is converted to decimal, and the nonce to hexadecimal (lowercase a-f).

The client stores the update request XML in a buffer, in UTF-8 format; it appends the keyid/nonce string to this buffer.  It calculates a SHA-256 hash of this combined buffer, which it stores for validation later.  It sends the update request and the keyid/nonce string to the server. _(It can optionally send the SHA-256 hash to the server as well; this is not necessary for proper operation of the protocol, but can be useful as a debugging aid.)_

The server receives an update request XML, public key id, and nonce; it performs the same appending operation, and computes the SHA-256 hash of the received data buffer. _(If the client sent the optional hash, it checks for equality, and emits a server log message on inequality.  It then discards the optional hash.)_

The server attempts to find a matching ECDSA private key for the specified public key id, returning an HTTP error if no such private key exists. Finally, it assembles the update response.

Before sending, the server stores the update response XML (also in UTF-8) in a buffer. It appends the computed SHA-256 hash of the request body+keyid+nonce to the buffer. It then calculates an ECDSA signature over that combined buffer, using the server’s private key. It sends the ECDSA signature and the response body + client hash back to the user.

The client receives the response XML, observed client hash, and ECDSA signature.  It concatenates its copy of the request hash to the response XML, and attempts to verify the ECDSA signature using its public key.  If the signature does not match, the client recognizes that the server response has been tampered in transit, and rejects the exchange.

The client then compares the SHA-256 hash in the response to the original hash of the request.  If the hashes do not match, the client recognizes that the request has been tampered in transit, and rejects the exchange.

_(Note: The hash compare can technically happen before the ECDSA verification, as long as both happen before accepting.)_

_(Note 2: The hash compare is technically optional.  The client could alternately choose to take the response XML and append its own copy of the request hash and nonce from the send; this will provide the same rejection quality.  Passing the request hash with the response is a low-cost addition that allows the client to identify which of the two parts of the exchange were modified: the request, the response, or both.)_

# Mapping Protocol Components to HTTP #

The mapping is fairly similar to the original CUP.  The update request will be sent as POST; the keyid, nonce, and SHA-256 hash will be transmitted as a query parameter in the URL.

The original CUP implementation used the "w=" query parameter to denote whether or not the body needs CUP validation.  This can be overloaded if desired; however, using a different query parameter allows an Omaha server to support both CUP and CUP-ECDSA from a single entry point.  The reference build of Omaha prefixes all CUPv2 params with “cup2.”

Formatting will be similar to CUP -- namely: “cup2key=%d:%u” where the first parameter is the keypair id, and the second is the client freshness nonce.

If the client chooses to send the optional SHA-256 hash for accidental tampering detection, it should convert it to a lowercase hexadecimal string (64 characters) and include it as a query parameter, such as “cup2hreq=...”.  Use the standard hex representation for SHA-256, which is the eight 32-bit blocks written out in big-endian and concatenated.

The server should return the ECDSA signature and client SHA-256 hash in the **ETag** HTTP header:

  * The signature consists of two 256-bit integers (“R” and “S”), in a ASN.1 sequence, encoded in DER; the hash is 256 bits.
  * Convert the DER-encoded signature to lowercase hex.  The SHA-256 hash will be standard hex representation.
  * Concatenate them with a colon as a delimiter: “signature:hash”.  The final ETag value will max out at 194 characters (plus \n), which is a bit long, but shouldn’t be risking the 8k limit on HTTP headers.