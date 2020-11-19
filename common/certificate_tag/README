certificate_tag.go is a tool for manipulating "tags" in Authenticode-signed,
Windows binaries.

Traditionally we have inserted tag data after the PKCS#7 blob in the file
(called an "appended tag" here). This area is not hashed in when checking
the signature so we can alter it at serving time without invalidating the
Authenticode signature.

However, Microsoft are changing the verification function to forbid that so
this tool also handles "superfluous certificate" tags. These are dummy
certificates, inserted into the PKCS#7 certificate chain, that can contain
arbitrary data in extensions. Since they are also not hashed when verifying
signatures, that data can also be changed without invalidating it.

More details are here: http://b/12236017

The tool was updated in 2020 to support MSI files: b/172261939, b/165818147.

The test file is integrated from google3, but is modified here to make it
easier to run outside of google3; see the comment near the beginning of
certificate_tag_test.go
