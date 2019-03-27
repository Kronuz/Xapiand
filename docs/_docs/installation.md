---
title: Installation
---

Getting {{ site.name }} installed and ready-to-go should only take a few minutes.
If it ever becomes a pain, please [file an issue]({{ site.repository }}/issues/new)
(or submit a pull request) describing the issue you encountered and how
we might make the process easier.


## Docker

```sh
~ $ docker pull dubalu/xapiand:{{ site.version }}
```

## Installation with Homebrew

Xapiand contains a formula for Homebrew (a package manager for OS X). It can
be installed by using the following command:

```sh
~ $ brew install Kronuz/tap/xapiand
```


## FreeBSD

```sh
Not yet available, build from sources.
```

There is a [FreeBSD port](https://github.com/Kronuz/Xapiand/blob/master/contrib/freebsd/xapiand.shar){:target="_blank"} available.


## Linux

```sh
Not yet available, build from sources.
```


## Building

You can also build and install from the sources if there's no other way. You can
find information about how to build from the sources [here]({{ '/docs/building' | relative_url }}).
