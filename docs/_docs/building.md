---
title: Building
permalink: /docs/building/
---

[GitHub]: https://github.com/Kronuz/Xapiand

To build from the sources, first you'll need to fork and clone the repository
from [GitHub]. Once you have a local copy, procede with the
[building process](#building-process).


## Requirements

Xapiand is written in C++14, it makes use of libev (which is included in the
codebase). The only external dependencies for building it are:

* Clang or GCC
* pkg-config
* CMake
* libpthread (internally used by the Standard C++ thread library)
* xapian-core v1.4+ (With patches by Kronuz applied, see [https://github.com/Kronuz/xapian])
* Optionally, Google's V8 Javascript engine library (tested with v5.1)


## Building process

1. Download and untar the Xapiand official distribution or clone repository
   from [GitHub].

2. Prepare build using:

```sh
~/Xapiand/src $ mkdir build
~/Xapiand/src/build $ cd build
~/Xapiand/src/build $ cmake -GNinja ..
```

3. build and install using:

```sh
~/Xapiand/src/build $ ninja
~/Xapiand/src/build $ ninja install
```

4. Run `xapiand` inside a new directory to be assigned to the node.

5. Run `curl 'http://localhost:8880/'`.


### Notes

* When preparing build for developing and debugging, generally you'd want to
  enable the Address Sanitizer, tracebacks in exceptions and debugging symbols:
  `cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DASAN=ON -DTRACEBACKS=ON ..`

* When compiling using ninja, the whole machine could slow down while compiling,
  since it uses all cores; you can prevent this by telling ninja to use
  `<number of cores> - 1` jobs. Example, for a system with 4 cores: `ninja -j3`.


#### macOS specifics


1. Simply installing Xcode will not install all of the command line developer
   tools, you must execute `xcode-select --install` in Terminal before trying
   to build.

2. You need cmake installed `brew install cmake`.

3. You need to request cmake to leave framework libraries last during the
   prepare build step above: `cmake -GNinja ..`


## Running the tests

```sh
~/Xapiand/src/build $ ninja check
```


## Installing

```sh
~/Xapiand/src/build $ ninja install
```
