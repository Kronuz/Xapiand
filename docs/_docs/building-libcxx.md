---
title: Building sanitized libc++
---

To build xapian and Xapiand with sanitizers support, both need to be compiled
with instrumented libc++ libraries. This documents how to build such
instrumented libc++.


## Preparing the source code

Download llvm and libc++ (it should be the same version your compiler is).

```sh
~/llvm $ mkdir -p src && cd src
~/llvm/src $ curl -LO https://releases.llvm.org/7.0.0/libunwind-7.0.0.src.tar.xz
~/llvm/src $ curl -LO https://releases.llvm.org/7.0.0/libcxxabi-7.0.0.src.tar.xz
~/llvm/src $ curl -LO https://releases.llvm.org/7.0.0/libcxx-7.0.0.src.tar.xz
~/llvm/src $ cd ..
```

Uncompress and configure directories

```sh
~/llvm $ for f in src/*.xz; do tar -xf "$f"; done
~/llvm $ ln -fs ../../libunwind-* libunwind
~/llvm $ ln -fs ../../libcxxabi-* libcxxabi
~/llvm $ ln -fs ../../libcxx-* libcxx
```

{: .note}
**_OS X: About error during cmake configure_**<br>
If receiving "Cannot find compiler-rt directory on OS X required for LLVM_USE_SANITIZER"
Make sure the directory inside the one returned by `clang++ -print-file-name=lib`
has the compiler-rt libraries in `darwin/libclang_rt.*`; those could be inside
echo `clang/7.0.0/lib/darwin` instead. (See the fix below)

Fix compiler-rt path:

```sh
ln -fs "$(clang++ -print-file-name=lib)/clang/"*"/lib/darwin" "$(clang++ -print-file-name=lib)"
```

{: .note}
**_OS X: About error during build_**<br>
If receiving "Undefined symbols for architecture x86_64: ___tsan_*" on OS X,
apply the patch for manually linking compiler-rt. (See the patch below)

Manually linking compiler-rt library (libunwind + libcxxabi):

```sh
cat <<"__EOF__" | patch -sup0
--- libunwind/CMakeLists.txt
+++ libunwind/CMakeLists.txt
@@ -233,6 +233,31 @@
   set(TARGET_TRIPLE "${LIBUNWIND_TARGET_TRIPLE}")
 endif()
 
+if (APPLE AND LLVM_USE_SANITIZER)
+  if (("${LLVM_USE_SANITIZER}" STREQUAL "Address") OR
+      ("${LLVM_USE_SANITIZER}" STREQUAL "Address;Undefined") OR
+      ("${LLVM_USE_SANITIZER}" STREQUAL "Undefined;Address"))
+    set(LIBFILE "libclang_rt.asan_osx_dynamic.dylib")
+  elseif("${LLVM_USE_SANITIZER}" STREQUAL "Undefined")
+    set(LIBFILE "libclang_rt.ubsan_osx_dynamic.dylib")
+  elseif("${LLVM_USE_SANITIZER}" STREQUAL "Thread")
+    set(LIBFILE "libclang_rt.tsan_osx_dynamic.dylib")
+  else()
+    message(WARNING "LLVM_USE_SANITIZER=${LLVM_USE_SANITIZER} is not supported on OS X")
+  endif()
+  if (LIBFILE)
+    find_compiler_rt_dir(LIBDIR)
+    if (NOT IS_DIRECTORY "${LIBDIR}")
+      message(FATAL_ERROR "Cannot find compiler-rt directory on OS X required for LLVM_USE_SANITIZER")
+    endif()
+    set(LIBUNWIND_SANITIZER_LIBRARY "${LIBDIR}/${LIBFILE}")
+    set(LIBUNWIND_SANITIZER_LIBRARY "${LIBUNWIND_SANITIZER_LIBRARY}" PARENT_SCOPE)
+    message(STATUS "Manually linking compiler-rt library: ${LIBUNWIND_SANITIZER_LIBRARY}")
+    list(APPEND LIBUNWINDCXX_ABI_LIBRARIES "${LIBUNWIND_SANITIZER_LIBRARY}")
+    list(APPEND LIBUNWIND_LINK_FLAGS "-Wl,-rpath,${LIBDIR}")
+  endif()
+endif()
+
 
 # Configure compiler.
 include(config-ix)
--- libcxxabi/CMakeLists.txt
+++ libcxxabi/CMakeLists.txt
@@ -249,6 +249,31 @@
   string(REPLACE "-stdlib=libstdc++" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
 endif()
 
+if (APPLE AND LLVM_USE_SANITIZER)
+  if (("${LLVM_USE_SANITIZER}" STREQUAL "Address") OR
+      ("${LLVM_USE_SANITIZER}" STREQUAL "Address;Undefined") OR
+      ("${LLVM_USE_SANITIZER}" STREQUAL "Undefined;Address"))
+    set(LIBFILE "libclang_rt.asan_osx_dynamic.dylib")
+  elseif("${LLVM_USE_SANITIZER}" STREQUAL "Undefined")
+    set(LIBFILE "libclang_rt.ubsan_osx_dynamic.dylib")
+  elseif("${LLVM_USE_SANITIZER}" STREQUAL "Thread")
+    set(LIBFILE "libclang_rt.tsan_osx_dynamic.dylib")
+  else()
+    message(WARNING "LLVM_USE_SANITIZER=${LLVM_USE_SANITIZER} is not supported on OS X")
+  endif()
+  if (LIBFILE)
+    find_compiler_rt_dir(LIBDIR)
+    if (NOT IS_DIRECTORY "${LIBDIR}")
+      message(FATAL_ERROR "Cannot find compiler-rt directory on OS X required for LLVM_USE_SANITIZER")
+    endif()
+    set(LIBCXXABI_SANITIZER_LIBRARY "${LIBDIR}/${LIBFILE}")
+    set(LIBCXXABI_SANITIZER_LIBRARY "${LIBCXXABI_SANITIZER_LIBRARY}" PARENT_SCOPE)
+    message(STATUS "Manually linking compiler-rt library: ${LIBCXXABI_SANITIZER_LIBRARY}")
+    add_library_flags("${LIBCXXABI_SANITIZER_LIBRARY}")
+    add_link_flags("-Wl,-rpath,${LIBDIR}")
+  endif()
+endif()
+
 if (LIBCXXABI_USE_COMPILER_RT)
   list(APPEND LIBCXXABI_LINK_FLAGS "-rtlib=compiler-rt")
 endif()
--- libcxx/lib/CMakeLists.txt
+++ libcxx/lib/CMakeLists.txt
@@ -169,9 +169,9 @@
           "-Wl,-reexport_library,${CMAKE_OSX_SYSROOT}/usr/lib/libc++abi.dylib")
       endif()
     else()
-      set(OSX_RE_EXPORT_LINE "/usr/lib/libc++abi.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi${LIBCXX_LIBCPPABI_VERSION}.exp")
+      set(OSX_RE_EXPORT_LINE "${LIBCXXABI_LIBCXX_LIBRARY_PATH}/libc++abi.${LIBCXX_ABI_VERSION}.0.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi${LIBCXX_LIBCPPABI_VERSION}.exp")
       if (NOT LIBCXX_ENABLE_NEW_DELETE_DEFINITIONS)
-        add_link_flags("/usr/lib/libc++abi.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi-new-delete.exp")
+        add_link_flags("${LIBCXXABI_LIBCXX_LIBRARY_PATH}/libc++abi.${LIBCXX_ABI_VERSION}.0.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi-new-delete.exp")
       endif()
     endif()
     add_link_flags(
__EOF__
```


## Building sanitized libc++

This is a common build function for each and all of the sanitized 
which follow.

```sh
install_libcxx() {
_sanitizer="$1"

if [ "$_sanitizer" = "Address" ]; then
    _suffix="-asan"
    _flags="-fno-omit-frame-pointer -fsanitize=address"
elif [ "$_sanitizer" = "Undefined" ]; then
    _suffix="-ubsan"
    _flags="-fno-omit-frame-pointer -fsanitize=undefined -fno-sanitize=vptr,function -fno-sanitize-recover=all"
elif [ "$_sanitizer" = "Address;Undefined" ] || [ "$_sanitizer" = "Undefined;Address" ]; then
    _suffix="-asan"
    _flags="-fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined -fno-sanitize=vptr,function -fno-sanitize-recover=all"
elif [ "$_sanitizer" = "Thread" ]; then
    _suffix="-tsan"
    _flags="-fno-omit-frame-pointer -fsanitize=thread"
elif [ "$_sanitizer" = "Memory" ]; then
    _suffix="-msan"
    _flags="-fno-omit-frame-pointer -fsanitize=memory"
elif [ "$_sanitizer" = "MemoryWithOrigins" ]; then
    _suffix="-msan"
    _flags="-fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins"
fi

_basedir="$(pwd)"
_libunwinddir="$_basedir/libunwind"
_libcxxdir="$_basedir/libcxx"
_libcxxabidir="$_basedir/libcxxabi"
_libdir="$_basedir/$_suffix/lib"


# Cleanup lib directory
rm -rf $_libdir
mkdir -p $_libdir


# Build and install libunwind
cd $_libunwinddir
rm -rf build$_suffix
mkdir build$_suffix
cd build$_suffix
ln -fs $_libdir
cmake .. \
    -Wno-dev \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$(dirname $(clang++ -print-file-name=lib))$_suffix" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="$_flags $CXXFLAGS" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_C_FLAGS="$_flags $CFLAGS" \
    -DLIBUNWIND_INCLUDE_TESTS=OFF \
    -DLIBUNWIND_INCLUDE_DOCS=OFF \
    -DLLVM_USE_SANITIZER=$_sanitizer
make -j4 install


# Build and install libc++abi
cd $_libcxxabidir
rm -rf build$_suffix
mkdir build$_suffix
cd build$_suffix
ln -fs $_libdir
cmake .. \
    -Wno-dev \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$(dirname $(clang++ -print-file-name=lib))$_suffix" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="$_flags $CXXFLAGS" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_C_FLAGS="$_flags $CFLAGS" \
    -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
    -DLIBCXXABI_LIBUNWIND_INCLUDES=/usr/include \
    -DLIBCXXABI_LIBCXX_INCLUDES="$_libcxxdir"/include \
    -DLIBCXXABI_LIBCXX_LIBRARY_PATH=$_libdir \
    -DLIBCXXABI_INCLUDE_TESTS=OFF \
    -DLIBCXXABI_INCLUDE_DOCS=OFF \
    -DLLVM_USE_SANITIZER=$_sanitizer
make -j4 install


# Build and install libc++
cd $_libcxxdir
rm -rf build$_suffix
mkdir build$_suffix
cd build$_suffix
ln -fs $_libdir
cmake .. \
    -Wno-dev \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="$(dirname $(clang++ -print-file-name=lib))$_suffix" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_CXX_FLAGS="$_flags $CXXFLAGS" \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_C_FLAGS="$_flags $CFLAGS" \
    -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
    -DLIBCXXABI_LIBUNWIND_INCLUDES=/usr/include \
    -DLIBCXXABI_LIBCXX_INCLUDES="$_libcxxdir"/include \
    -DLIBCXXABI_LIBCXX_LIBRARY_PATH=$_libdir \
    -DLIBCXXABI_INCLUDE_TESTS=OFF \
    -DLIBCXXABI_INCLUDE_DOCS=OFF \
    -DLIBCXX_CXX_ABI=libcxxabi \
    -DLIBCXX_CXX_ABI_INCLUDE_PATHS="$_libcxxabidir"/include \
    -DLIBCXX_CXX_ABI_LIBRARY_PATH=$_libdir \
    -DLIBCXX_ENABLE_ASSERTIONS=ON \
    -DLIBCXX_BENCHMARK_NATIVE_STDLIB=OFF \
    -DLIBCXX_INCLUDE_TESTS=OFF \
    -DLIBCXX_INCLUDE_DOCS=OFF \
    -DLLVM_USE_SANITIZER=$_sanitizer
make -j4 install

}
```


### Address Sanitizer (ASAN)

```sh
install_libcxx "Address;Undefined"
```


### Thread Sanitizer (TSAN)

```sh
install_libcxx "Thread"
```


### Memory Sanitizer (MSAN)

```sh
install_libcxx "MemoryWithOrigins"
```


### Usage

Use by setting LDFLAGS, ex.:

```sh
export LDFLAGS="-L$(dirname $(clang++ -print-file-name=lib))-tsan/lib -Wl,-rpath,$(dirname $(clang++ -print-file-name=lib))-tsan/lib"
```
