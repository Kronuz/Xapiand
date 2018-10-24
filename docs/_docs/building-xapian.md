---
title: Building Xapian Library
---

[GitHub]: https://github.com/xapian/xapian
[Kronuz Fork]: https://github.com/Kronuz/xapian

To build xapian from the sources, first you'll need to fork and clone the
repository from [GitHub] or from [Kronuz Fork]. Once you have a local copy,
procede with the [building process](#building-process).


### Requirements

Xapian is written in C++11 and it has the following build requirements:

* Clang or GCC
* pkg-config
* automake
* libtool
* tcl86


### Dependencies

The only external dependencies for building it are:

* zlib


## Building process

#### Get the Sources

First download and untar the xapian official distribution or clone the
repository from [https://github.com/xapian/xapian.git](https://github.com/xapian/xapian.git)

```sh
~/ $ git clone -b RELEASE/1.4 --single-branch --depth 1 \
  "https://github.com/xapian/xapian.git"
```

#### Prepare the Build

```sh
~/ $ cd xapian/xapian-core
~/xapian/xapian-core $ ./preautoreconf
~/xapian/xapian-core $ autoreconf --force --install -Im4 -I/usr/local/share/aclocal
```

#### Configure the Build

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/HEAD' || echo '/usr/local')" \
  CXXFLAGS="-DFLINTLOCK_USE_FLOCK $CXXFLAGS"
```

#### Build, Test and Install

```sh
~/xapian/xapian-core $ make
~/xapian/xapian-core $ make check
~/xapian/xapian-core $ make install
```


## Building sanitized libraries

For building sanitized versions of the library, replace **Configure the Build**
step above and replace accordingly with the following:

{: .note}
**_Sanitized libc++ needed!_**<br>
Sanitized versions of libc++ are needed for these builds to be reliable.


### Address Sanitizer (ASAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/ASAN' || echo '/usr/local')" \
  CXXFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=address -DFLINTLOCK_USE_FLOCK $CXXFLAGS" \
  LDFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=address $LDFLAGS"
```


### Memory Sanitizer (MSAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/MSAN' || echo '/usr/local')" \
  CXXFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=memory -fsanitize-memory-track-origins -DFLINTLOCK_USE_FLOCK $CXXFLAGS" \
  LDFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=memory -fsanitize-memory-track-origins $LDFLAGS"
```


### Undefined Behavior Sanitizer (UBSAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/UBSAN' || echo '/usr/local')" \
  CXXFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=undefined -fno-sanitize=vptr,function -fno-sanitize-recover=all -DFLINTLOCK_USE_FLOCK $CXXFLAGS" \
  LDFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=undefined -fno-sanitize=vptr,function -fno-sanitize-recover=all $LDFLAGS"
```


### Thread Sanitizer (TSAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/TSAN' || echo '/usr/local')" \
  CXXFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=thread -DFLINTLOCK_USE_FLOCK $CXXFLAGS" \
  LDFLAGS="-fno-omit-frame-pointer -gline-tables-only -fsanitize=thread $LDFLAGS"
```
