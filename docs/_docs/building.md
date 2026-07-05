---
title: Building from Sources
short_title: Building
---

[GitHub]: https://github.com/Kronuz/Xapiand

To build from the sources, first you'll need to fork and clone the repository
from [GitHub]. Once you have a local copy, proceed with the
[building process](#building-process).


### Requirements

Xapiand is written in C++20 and it has the following build requirements:

* pkg-config
* Ninja (optional)
* A C++20 compiler: Clang >= 14 (Apple Clang >= 14) or GCC >= 13
* CMake >= 3.14 (FetchContent is used heavily)
* perl >= 5.6 (for a few building scripts)
* Tcl >= 8.6  (to generate unicode/unicode-data.cc)


---
### Dependencies

Xapiand makes use of quite a few libraries. Its search engine is a vendored fork
of [Xapian](https://xapian.org/){:target="_blank"} 2.0.0, built as part of the tree,
and most of the remaining building blocks (MessagePack, the storage codecs, logging,
the Asio-based reactor runtime, the [Lua](https://www.lua.org/){:target="_blank"}
scripting engine via sol2, and more) are pulled in automatically at configure time
through CMake's `FetchContent` from the standalone Kronuz libraries, so **a network
connection is required the first time you configure**. (Earlier releases embedded
ChaiScript and a libev event loop; those are now Lua and the Asio reactor.)

The only external system dependencies you need to provide are:

* zlib
* libpthread (internally used by the Standard C++ thread library)
* ICU >= 54.1 (optional)


---
#### macOS

To install the requirements under macOS you need:

##### 1. Configure Xcode

Simply installing Xcode will not install all of the command line developer
tools, the first time you must execute the following in Terminal, before trying
to build:

{:class="plat_osx"}

```sh
# Install the command line developer tools:
~/ $ xcode-select --install
```

##### 2. [Install Homebrew](https://docs.brew.sh/Installation){:target="_blank"}

##### 3. Install Requirements

{:class="plat_osx"}

```sh
~/ $ brew install ninja
~/ $ brew install pkg-config
~/ $ brew install cmake
~/ $ brew install icu
```


---
## Building process

#### Get the Sources

First download and untar the Xapiand official distribution or clone the
repository from [https://github.com/Kronuz/Xapiand.git](https://github.com/Kronuz/Xapiand.git)

```sh
~/ $ git clone -b master --single-branch --depth 1 \
  "https://github.com/Kronuz/Xapiand.git"
```

#### Prepare the Build

```sh
~/ $ cd Xapiand
~/Xapiand $ mkdir build
~/Xapiand $ cd build
```

#### Configure the Build

```sh
~/Xapiand/build $ cmake -GNinja ..
```

{: .note .info }
The default build type is an optimized **Release** with **Link-Time Optimization
(LTO)** enabled (the `LTO` option defaults on). The first configure downloads the
fetched dependencies (see [Dependencies](#dependencies)), which can take a few
minutes; subsequent configures reuse them.

#### Build, Test and Install

```sh
~/Xapiand/build $ ninja
~/Xapiand/build $ ninja check
~/Xapiand/build $ ninja install
```

{: .note .tip }
**_CPU Usage_**<br>
When compiling using ninja, the whole machine could slow down while compiling,
as `ninja`, by default, uses all available CPU cores; you can prevent this by
telling ninja to use `<number of cores> - 1` jobs. Example, for a system with
4 CPU cores: `ninja -j3`.


---
## Sanitizers

When building sanitized versions of Xapiand, you'll need to
[Configure the Build](#configure-the-build) using the proper library:

{: .note .caution }
**_On macOS, build the sanitized binaries with Homebrew's LLVM, not Apple Clang._**<br>
Recent Apple Clang toolchains ship an Address/Thread Sanitizer runtime that can hang
on startup on Apple silicon. Install it with `brew install llvm` and point the build
at it, for example
`CC=/opt/homebrew/opt/llvm/bin/clang` `CXX=/opt/homebrew/opt/llvm/bin/clang++` `cmake -GNinja -DASAN=ON ..`,
then run the resulting binary with `DYLD_LIBRARY_PATH=/opt/homebrew/opt/llvm/lib`.


### Address Sanitizer (ASAN)

For developing and debugging, generally you'd want to enable the
*Address Sanitizer*, tracebacks in exceptions and debugging symbols,
so you'll have to **Configure the Build** using something like:

```sh
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DASAN=ON ..
```


### UndefinedBehavior Sanitizer (ASAN + UBSAN)

For developing and debugging, generally you'd want to enable the
*Address Sanitizer* and *UndefinedBehavior Sanitizer*, tracebacks in
exceptions and debugging symbols, so you'll have to **Configure the Build**
using something like:

```sh
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DASAN=ON -DUBSAN=ON ..
```


### Memory Sanitizer (MSAN)

For debugging memory issues, enable *Memory Sanitizer* and debugging
symbols in release mode, so you'll have to **Configure the Build** using
something like:

```sh
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DMSAN=ON ..
```


### Thread Sanitizer (TSAN)

For debugging multithread issues, enable *Thread Sanitizer* and debugging
symbols in release mode, so you'll have to **Configure the Build** using
something like:

```sh
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DTSAN=ON ..
```
