---
title: Building from Sources
short_title: Building
---

[GitHub]: https://github.com/Kronuz/Xapiand

To build from the sources, first you'll need to fork and clone the repository
from [GitHub]. Once you have a local copy, procede with the
[building process](#building-process).


### Requirements

Xapiand is written in C++17 and it has the following build requirements:

* pkg-config
* Ninja (optional)
* Clang >= 5.0 or GCC >= 7.0
* CMake >= 3.12
* perl >= 5.6 (for a few building scripts)
* Tcl >= 8.6  (to generate unicode/unicode-data.cc)


---
### Dependencies

Xapiand it makes use a quite few libraries: libev, Chaiscript, Xapian, LZ4,
but most of them are all included in the codebase. The only external
dependencies needed for building it are:

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
# Install command tools using:
~/ $ sudo xcode-select --install
~/ $ sudo open /Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg
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
