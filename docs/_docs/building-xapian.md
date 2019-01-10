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
~/ $ git clone -b master --single-branch --depth 1 \
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
  --program-suffix='' \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/1.5' || echo '/usr/local')" \
  CXXFLAGS="-O3 -DFLINTLOCK_USE_FLOCK -DXAPIAN_MOVE_SEMANTICS $CXXFLAGS" \
  LDFLAGS="-O3 $LDFLAGS"
```

#### Build, Test and Install

```sh
~/xapian/xapian-core $ make
~/xapian/xapian-core $ make check
~/xapian/xapian-core $ make install
```


### Debug mode

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --enable-assertions \
  --program-suffix="" \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/1.5' || echo '/usr/local')" \
  CXXFLAGS="-O2 -g -DFLINTLOCK_USE_FLOCK -DXAPIAN_MOVE_SEMANTICS $CXXFLAGS" \
  LDFLAGS="-O2 -g $LDFLAGS"
```


### Address Sanitizer (ASAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --enable-assertions \
  --program-suffix="" \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/1.5-asan' || echo '/usr/local')" \
  CXXFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=address -DFLINTLOCK_USE_FLOCK -DXAPIAN_MOVE_SEMANTICS $CXXFLAGS" \
  LDFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=address $LDFLAGS"
```


### UndefinedBehavior Sanitizer (ASAN + UBSAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --enable-assertions \
  --program-suffix="" \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/1.5-ubsan' || echo '/usr/local')" \
  CXXFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -fno-sanitize=vptr,function -fno-sanitize-recover=all -DFLINTLOCK_USE_FLOCK -DXAPIAN_MOVE_SEMANTICS $CXXFLAGS" \
  LDFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -fno-sanitize=vptr,function -fno-sanitize-recover=all $LDFLAGS"
```


### Memory Sanitizer (MSAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --enable-assertions \
  --program-suffix="" \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/1.5-msan' || echo '/usr/local')" \
  CXXFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -DFLINTLOCK_USE_FLOCK -DXAPIAN_MOVE_SEMANTICS $CXXFLAGS" \
  LDFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins $LDFLAGS"
```


### Thread Sanitizer (TSAN)

```sh
~/xapian/xapian-core $ ./configure \
  --disable-dependency-tracking \
  --disable-documentation \
  --enable-maintainer-mode \
  --enable-assertions \
  --program-suffix="" \
  --prefix="$([ -d '/usr/local/Cellar' ] && echo '/usr/local/Cellar/xapian/1.5-tsan' || echo '/usr/local')" \
  CXXFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=thread -DFLINTLOCK_USE_FLOCK -DXAPIAN_MOVE_SEMANTICS $CXXFLAGS" \
  LDFLAGS="-O2 -g -fno-omit-frame-pointer -fsanitize=thread $LDFLAGS"
```
