---
title: "Building from Sources"
---

[GitHub]: https://github.com/Kronuz/Xapiand

To build from the sources, first you'll need to fork and clone the repository
from [GitHub]. Once you have a local copy, proceed with the
[building process](#building-process).

### Requirements

Xapiand is written in C++20 and it has the following build requirements:

* pkg-config
* Ninja (optional)
* A C++20 compiler. The CI builds and the Docker image use GCC 13 on Linux, and
  the Homebrew bottles use Apple Clang on macOS; older compilers are not tested.
* CMake 3.14 or newer. `CMakeLists.txt` declares 3.12 as its minimum, but it
  uses `FetchContent_MakeAvailable`, which CMake provides from 3.14, and
  `FetchContent` is how the libraries are fetched.

---
### Dependencies

Xapiand makes use of quite a few libraries. Its search engine is
[strata](https://github.com/Kronuz/strata), reached through the
[prospect](https://github.com/Kronuz/prospect) search layout, and it and the
remaining building blocks (MessagePack, the storage codecs, logging, the
Asio-based reactor runtime, the [Lua](https://www.lua.org/) scripting engine via
sol2, and more) are pulled in automatically at configure time through CMake's
`FetchContent` from the [standalone Kronuz libraries](/Xapiand/architecture/dependencies), so **a network
connection is required the first time you configure**.

The external system dependencies you need to provide are:

* zstd (required)
* libuuid (required on Linux, `uuid-dev` or `libuuid-devel`; macOS and FreeBSD
  use the system's UUID functions)
* OpenSSL 3 (required unless you configure with `-DTLS=OFF`; TLS is on by default)
* zlib (used when found)
* libcap (used when found, Linux only)
* libpthread (internally used by the Standard C++ thread library)

[Asio](https://think-async.com/Asio/) is header-only and is fetched at configure
time, unless you point `ASIO_INCLUDE_DIR` at an existing copy. On Ubuntu 24.04,
the packages the Dockerfile builds with are `g++-13 cmake ninja-build pkg-config
libasio-dev libzstd-dev uuid-dev zlib1g-dev libcap-dev libssl-dev`.

---
#### macOS

To install the requirements under macOS you need:

##### 1. Configure Xcode

Simply installing Xcode will not install all of the command line developer
tools, the first time you must execute the following in Terminal, before trying
to build:

```sh
# Install the command line developer tools:
xcode-select --install
```

##### 2. [Install Homebrew](https://docs.brew.sh/Installation)

##### 3. Install Requirements

```sh
brew install ninja
brew install pkg-config
brew install cmake
brew install openssl@3
brew install zstd
```

---
## Building process

#### Get the Sources

First download and untar the Xapiand official distribution or clone the
repository from [https://github.com/Kronuz/Xapiand.git](https://github.com/Kronuz/Xapiand.git)

```sh
git clone -b master --single-branch --depth 1 \
  "https://github.com/Kronuz/Xapiand.git"
```

#### Prepare the Build

```sh
cd Xapiand
mkdir build
cd build
```

#### Configure the Build

```sh
cmake -GNinja -DBUILD_TESTS=ON ..
```

`BUILD_TESTS` is only needed to run the unit tests (`ninja check`, below).

:::hint{.info}
The default build type is an optimized **Release** with **Link-Time Optimization
(LTO)** enabled (the `LTO` option defaults on). The first configure downloads the
fetched dependencies (see [Dependencies](#dependencies)), which can take a few
minutes; subsequent configures reuse them.
:::

#### Build, Test and Install

```sh
ninja
ninja check    # needs the BUILD_TESTS=ON configure above
ninja install
```

The binary is `bin/xapiand` in the build directory. The scripts in `harness/`
run the node end to end: `harness/run_all.sh smoke e2e` starts a node from
`build/bin/xapiand` and runs a smoke test and every request example of this
documentation against it (the `e2e` layer needs newman 6 or newer on the `PATH`).

:::hint[CPU Usage]{.tip}
When compiling using ninja, the whole machine could slow down while compiling,
as `ninja`, by default, uses all available CPU cores; you can prevent this by
telling ninja to use `<number of cores> - 1` jobs. Example, for a system with
4 CPU cores: `ninja -j3`.
:::

---
## Build Options

The build is configured with CMake options, toggled at configure time with
`-D<OPTION>=ON` / `-D<OPTION>=OFF`. The defaults produce an optimized
release; you rarely need to change them for a normal build.

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | `Release` | Build type (`Release`, `RelWithDebInfo`, `Debug`, …) |
| `LTO` | `ON` | Link-Time Optimization |
| `TLS` | `ON` | HTTPS for the REST API (`--tls-cert`, `--tls-key`); needs OpenSSL 3 |
| `LUA` | `ON` | The Lua scripting engine (`_script`) |
| `DATABASE_WAL` | `ON` | Database write-ahead log (WAL) |
| `DATA_STORAGE` | `ON` | Data storage volumes |
| `UUID_ENCODED` | `ON` | Allow the encoded (base59) UUID representation (`--uuid`) |
| `UUID_GUID` | `OFF` | Allow the GUID UUID representation (`--uuid`) |
| `UUID_URN` | `OFF` | Allow the URN UUID representation (`--uuid`) |
| `TRACEBACKS` | `OFF` (`ON` in Debug) | Tracebacks in exceptions |
| `ASSERTS` | `OFF` (`ON` in Debug) | Runtime assertions |
| `TRACKED_MEM` | `OFF` | Allocator that attributes memory to call sites |
| `BUILD_TESTS` | `OFF` | Build the test suite, and the `check` target that runs it |
| `ASAN` | `OFF` | AddressSanitizer (see [Sanitizers](#sanitizers)) |
| `UBSAN` | `OFF` | UndefinedBehaviorSanitizer |
| `MSAN` | `OFF` | MemorySanitizer |
| `TSAN` | `OFF` | ThreadSanitizer |
| `DEBUGINFO_EXTERNALIZE` | `OFF` | Externalize debug information (macOS) |

These options are for development and testing, and most users never set them:

| Option | Default | Description |
|--------|---------|-------------|
| `FUZZ` | `OFF` | Build the libFuzzer targets (needs a clang with libFuzzer) |
| `CRASH_POINTS` | `OFF` | Let `XAPIAND_STRATA_CRASH=point:n` kill the process at a strata commit or replay point, for crash tests |
| `ENGINE_CENSUS` | `OFF` | Count the facilities each request uses at the engine seam (`XAPIAND_ENGINE_CENSUS=file`) |
| `RANDOM_ERRORS` | `OFF` | Add the `--random-errors-net`, `--random-errors-io` and `--random-errors-db` options, which inject random errors |
| `CHECK_IO_FDES` | `OFF` | Check file descriptors |
| `CHECK_SIZES` | `OFF` | Print the objects sizes |

For example, a debug build with tracebacks and assertions:

```sh
cmake -GNinja -DCMAKE_BUILD_TYPE=Debug -DTRACEBACKS=ON -DASSERTS=ON ..
```

---
## Sanitizers

When building sanitized versions of Xapiand, you'll need to
[Configure the Build](#configure-the-build) using the proper library:

:::hint[On macOS, build the sanitized binaries with Homebrew's LLVM, not Apple Clang.]{.caution}
Recent Apple Clang toolchains ship an Address/Thread Sanitizer runtime that can hang
on startup on Apple silicon. Install it with `brew install llvm` and point the build
at it, for example
`CC=/opt/homebrew/opt/llvm/bin/clang` `CXX=/opt/homebrew/opt/llvm/bin/clang++` `cmake -GNinja -DASAN=ON ..`,
then run the resulting binary with `DYLD_LIBRARY_PATH=/opt/homebrew/opt/llvm/lib`.
:::

### Address Sanitizer (ASAN)

For developing and debugging, generally you'd want to enable the
*Address Sanitizer*, tracebacks in exceptions and debugging symbols,
so you'll have to **Configure the Build** using something like:

```sh
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DASAN=ON ..
```

### UndefinedBehavior Sanitizer (ASAN + UBSAN)

For developing and debugging, generally you'd want to enable the
*Address Sanitizer* and *UndefinedBehavior Sanitizer*, tracebacks in
exceptions and debugging symbols, so you'll have to **Configure the Build**
using something like:

```sh
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DASAN=ON -DUBSAN=ON ..
```

### Memory Sanitizer (MSAN)

For debugging memory issues, enable *Memory Sanitizer* and debugging
symbols in release mode, so you'll have to **Configure the Build** using
something like:

```sh
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DMSAN=ON ..
```

### Thread Sanitizer (TSAN)

For debugging multithread issues, enable *Thread Sanitizer* and debugging
symbols in release mode, so you'll have to **Configure the Build** using
something like:

```sh
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DTRACEBACKS=ON -DASSERTS=ON -DTSAN=ON ..
```

---
## See Also

- [Architecture Overview](/Xapiand/architecture) — the layers and how a request flows through them.
- [Dependencies](/Xapiand/architecture/dependencies) — every fetched library, with links and a transitive tree.
- [Internals](/Xapiand/architecture/internals) — source layout, conventions, and load-bearing invariants.
