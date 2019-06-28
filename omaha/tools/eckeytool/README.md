This tool converts an ECDSA public key in a PEM format to a CUP-ECDSA public key
in a text format, which can be included as source code in the Omaha project
tree.

This directory is not built as part of the Omaha tree. Instead, build this code
on Linux, using the provide makefile. The code provided has a dependency on
openssl. To generate a key, use the following steps:
* generate an EC private key using openssl:
  `openssl ecparam -out priv.pem -name prime256v1 -genkey`
* extract the corresponding EC public key:
  `openssl ec -in priv.pem -pubout -out public.pem`
* convert the EC public to a CUP ECDSA key: `./eckeytool public.pem 1`
* take the output of the eckeytool and add it to your project tree.
