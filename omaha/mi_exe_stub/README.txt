This directory contains the source needed to build the Omaha meta-installer EXE, which is the installer that installs Omaha. We call it a meta-installer to distinguish it from the Omaha update client, which itself is an installer.

This project differs from the mi_msi project solely in that it's an EXE rather than an MSI. Some customers need one, and some need the other.