---
title: "Installation"
---

Getting Xapiand installed and ready-to-go should only take a few minutes.
If it ever becomes a pain, please [file an issue](https://github.com/Kronuz/Xapiand/issues/new)
(or submit a pull request) describing the issue you encountered and how
we might make the process easier.

## Docker

```sh
~ $ docker pull ghcr.io/kronuz/xapiand:latest
```

## Installation with Homebrew

Xapiand contains a formula for Homebrew (a package manager for OS X). It can
be installed by using the following command:

```sh
~ $ brew install Kronuz/tap/xapiand
```

## FreeBSD

A native FreeBSD package (`amd64`, built for FreeBSD 14) is attached to each
[release](https://github.com/Kronuz/Xapiand/releases). Download it and install
the local package file with:

```sh
~ $ pkg add ./xapiand-*.pkg
```

## Linux

RPM (`x86_64`, `aarch64`) and DEB (`amd64`, `arm64`) packages are attached to
each [release](https://github.com/Kronuz/Xapiand/releases). Download the one
matching your distribution and architecture and install it:

```sh
# RPM-based (Fedora, RHEL, CentOS, ...):
~ $ sudo rpm -i xapiand-*.x86_64.rpm

# DEB-based (Debian 13+, Ubuntu 24.04+):
~ $ sudo dpkg -i xapiand_*_amd64.deb
```

A multi-arch [Docker image](#docker) is also available for any distribution.

## Building

You can also build and install from the sources if there's no other way. You can
find information about how to build from the sources [here](/Xapiand/building).
