---
title: Building from Sources
---

[GitHub]: https://github.com/Kronuz/Xapiand

To build from the sources, first you'll need to fork and clone the repository
from [GitHub]. Once you have a local copy, procede with the
[building process](#building-process).


### Requirements

Xapiand is written in C++14, it makes use of libev (which is included in the
codebase), it has the following build requirements:

* Clang v4.0+
* pkg-config
* CMake


### Dependencies

The only external dependencies for building it are:

* zlib
* libpthread (internally used by the Standard C++ thread library)
* xapian-core v1.4+ (Optionally, with patches by Kronuz applied, see [https://github.com/Kronuz/xapian])


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


### Notes

* When compiling using ninja, the whole machine could slow down while compiling,
  as `ninja`, by default, uses all available CPU cores; you can prevent this by
  telling ninja to use `<number of cores> - 1` jobs. Example, for a system with
  4 CPU cores: `ninja -j3`.

* When building sanitized versions of Xapiand, you'll need
  [sanitized versions of xapian]({{ '/docs/building-xapian/#building-sanitized-libraries' | relative_url }})
  and **Configure the Build** using the proper library (for ASAN, for example):

    ```sh
~/Xapiand/build $ cmake -GNinja -DASAN=ON ..
```

#### Address Sanitizer (ASAN)

* For developing and debugging, generally you'd want to enable the
  *Address Sanitizer*, tracebacks in exceptions and debugging symbols,
  so you'll have to **Configure the Build** using something like:

    ```sh
~/Xapiand/build $ brew switch xapian 1.5-asan
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DASAN=ON ..
```

#### UndefinedBehavior Sanitizer (ASAN + UBSAN)

* For developing and debugging, generally you'd want to enable the
  *Address Sanitizer* and *UndefinedBehavior Sanitizer*, tracebacks in
  exceptions and debugging symbols, so you'll have to **Configure the Build**
  using something like:

    ```sh
~/Xapiand/build $ brew switch xapian 1.5-asan
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DASAN=ON -DUBSAN=ON ..
```

#### Memory Sanitizer (MSAN)

* For debugging memory issues, enable *Memory Sanitizer* and debugging
  symbols in release mode, so you'll have to **Configure the Build** using
  something like:

    ```sh
~/Xapiand/build $ brew switch xapian 1.5-msan
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DMSAN=ON ..
```

#### Thread Sanitizer (TSAN)

* For debugging multithread issues, enable *Thread Sanitizer* and debugging
  symbols in release mode, so you'll have to **Configure the Build** using
  something like:

    ```sh
~/Xapiand/build $ brew switch xapian 1.5-tsan
~/Xapiand/build $ cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DTSAN=ON ..
```

#### macOS specifics


1. Simply installing Xcode will not install all of the command line developer
   tools, the first time you must execute `xcode-select --install` in Terminal
   before trying to build.

2. You need CMake installed `brew install cmake`.
