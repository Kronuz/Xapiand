---
title: Installation
---

Getting Xapiand installed and ready-to-go should only take a few minutes.
If it ever becomes a pain, please [file an issue]({{ site.repository }}/issues/new)
(or submit a pull request) describing the issue you encountered and how
we might make the process easier.


## Docker

```sh
~ $ docker pull ghcr.io/kronuz/xapiand:{{ site.version }}
```

## Installation with Homebrew

Xapiand contains a formula for Homebrew (a package manager for OS X). It can
be installed by using the following command:

{:class="plat_osx"}

```sh
~ $ brew install Kronuz/tap/xapiand
```


## FreeBSD

{:class="plat_linux"}

```sh
Not yet available, build from sources.
```

There is a [FreeBSD port](https://github.com/Kronuz/Xapiand/blob/master/contrib/freebsd/xapiand.shar){:target="_blank"} available.


## Linux

{:class="plat_linux"}

RPM packages are attached to each [release]({{ site.repository }}/releases). Download
the one matching your distribution and install it:

```sh
~ $ sudo rpm -i xapiand-{{ site.version }}-1.x86_64.rpm
```

A statically-linked `x86_64` binary is also attached to every release for
distributions without RPM.


## Building

You can also build and install from the sources if there's no other way. You can
find information about how to build from the sources [here]({{ '/docs/building' | relative_url }}).
