---
title: Building sanitized libc++
---

To build xapian and Xapiand with sanitizers support, both need to be compiled
with instrumented libc++ libraries. This documents how to build such
instrumented libc++.


## Building sanitized libc++

Download llvm and libc++ (it should be the same version your compiler is).

```sh
~/llvm $ mkdir -p src && cd src
~/llvm/src $ curl -LO https://releases.llvm.org/7.0.0/llvm-7.0.0.src.tar.xz
~/llvm/src $ curl -LO https://releases.llvm.org/7.0.0/libcxx-7.0.0.src.tar.xz
~/llvm/src $ curl -LO https://releases.llvm.org/7.0.0/libcxxabi-7.0.0.src.tar.xz
~/llvm/src $ curl -LO https://releases.llvm.org/7.0.0/libunwind-7.0.0.src.tar.xz
~/llvm/src $ cd ..
```

Uncompress and configure directories

```sh
~/llvm $ for f in src/*.xz; do tar -xf "$f"; done
~/llvm $ ln -fs llvm-7.0.0.src llvm
~/llvm/llvm/projects $ cd llvm/projects
~/llvm/llvm/projects $ ln -fs ../../libcxx-* libcxx
~/llvm/llvm/projects $ ln -fs ../../libcxxabi-* libcxxabi
~/llvm/llvm/projects $ ln -fs ../../libunwind-* libunwind
~/llvm/llvm/projects $ cd ../..
```


{: .note}
**_OS X: About error during cmake configure_**<br>
If receiving "Cannot find compiler-rt directory on OS X required for LLVM_USE_SANITIZER"
Make sure the directory inside the one returned by `clang++ -print-file-name=lib`
has the compiler-rt libraries in `darwin/libclang_rt.*`; those could be inside
echo `clang/7.0.0/lib/darwin` instead. (See the fix below)

Fix compiler-rt path:

```sh
ln -fs "$(clang++ -print-file-name=lib)/clang/*/lib/darwin" "$(clang++ -print-file-name=lib)"
```


{: .note}
**_OS X: About error during build_**<br>
If receiving "Undefined symbols for architecture x86_64: ___tsan_*" on OS X,
apply the patch for manually linking compiler-rt. (See the patch below)

Manually linking compiler-rt library (libunwind + libcxxabi):

```sh
cat <<"__EOF__" | patch -sup0
--- llvm/projects/libunwind/CMakeLists.txt
+++ llvm/projects/libunwind/CMakeLists.txt
@@ -233,6 +233,31 @@
 # Configure compiler.
 include(config-ix)
 
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
 if (LIBUNWIND_USE_COMPILER_RT)
   list(APPEND LIBUNWIND_LINK_FLAGS "-rtlib=compiler-rt")
 endif()
--- llvm/projects/libcxx/lib/CMakeLists.txt
+++ llvm/projects/libcxx/lib/CMakeLists.txt
@@ -169,9 +169,9 @@
           "-Wl,-reexport_library,${CMAKE_OSX_SYSROOT}/usr/lib/libc++abi.dylib")
       endif()
     else()
-      set(OSX_RE_EXPORT_LINE "/usr/lib/libc++abi.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi${LIBCXX_LIBCPPABI_VERSION}.exp")
+      set(OSX_RE_EXPORT_LINE "lib/libc++abi.${LIBCXX_ABI_VERSION}.0.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi${LIBCXX_LIBCPPABI_VERSION}.exp")
       if (NOT LIBCXX_ENABLE_NEW_DELETE_DEFINITIONS)
-        add_link_flags("/usr/lib/libc++abi.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi-new-delete.exp")
+        add_link_flags("lib/libc++abi.${LIBCXX_ABI_VERSION}.0.dylib -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libc++abi-new-delete.exp")
       endif()
     endif()
     add_link_flags(
--- llvm/projects/libcxxabi/CMakeLists.txt
+++ llvm/projects/libcxxabi/CMakeLists.txt
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
+    list(APPEND LIBCXXABI_LIBRARIES "${LIBCXXABI_SANITIZER_LIBRARY}")
+    list(APPEND LIBCXXABI_LINK_FLAGS "-Wl,-rpath,${LIBDIR}")
+  endif()
+endif()
+
 if (LIBCXXABI_USE_COMPILER_RT)
   list(APPEND LIBCXXABI_LINK_FLAGS "-rtlib=compiler-rt")
 endif()
__EOF__
```


### Address Sanitizer (ASAN)

```sh
~/llvm $ mkdir -p build/asan && cd build/asan
~/llvm/build/asan $ cmake ../../llvm \
  -GNinja \
  -DCMAKE_INSTALL_PREFIX="$(dirname $(clang++ -print-file-name=lib))-asan" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_USE_SANITIZER=Address \
  -DLLVM_INCLUDE_TESTS=OFF
~/llvm/build/asan $ ninja install-unwind install-libcxxabi install-libcxx
~/llvm/build/asan $ cd ../..
```


### Memory Sanitizer (MSAN)

```sh
~/llvm $ mkdir -p build/msan && cd build/msan
~/llvm/build/msan $ cmake ../../llvm \
  -GNinja \
  -DCMAKE_INSTALL_PREFIX="$(dirname $(clang++ -print-file-name=lib))-msan" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_USE_SANITIZER=MemoryWithOrigins \
  -DLLVM_INCLUDE_TESTS=OFF
~/llvm/build/msan $ ninja install-unwind install-libcxxabi install-libcxx
~/llvm/build/msan $ cd ../..
```


### Undefined Behavior Sanitizer (UBSAN)

```sh
~/llvm $ mkdir -p build/ubsan && cd build/ubsan
~/llvm/build/ubsan $ cmake ../../llvm \
  -GNinja \
  -DCMAKE_INSTALL_PREFIX="$(dirname $(clang++ -print-file-name=lib))-ubsan" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_USE_SANITIZER=Undefined \
  -DLLVM_INCLUDE_TESTS=OFF
~/llvm/build/ubsan $ ninja install-unwind install-libcxxabi install-libcxx
~/llvm/build/ubsan $ cd ../..
```


### Thread Sanitizer (TSAN)

```sh
~/llvm $ mkdir -p build/tsan && cd build/tsan
~/llvm/build/tsan $ cmake ../../llvm \
  -GNinja \
  -DCMAKE_INSTALL_PREFIX="$(dirname $(clang++ -print-file-name=lib))-tsan" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_USE_SANITIZER=Thread \
  -DLLVM_INCLUDE_TESTS=OFF
~/llvm/build/tsan $ ninja install-unwind install-libcxxabi install-libcxx
~/llvm/build/tsan $ cd ../..
```


### Usage

Use by setting LDFLAGS, ex.:

```sh
LDFLAGS='-L/usr/local/opt/llvm@7-tsan/lib -Wl,-rpath,/usr/local/opt/llvm@7-tsan/lib -L/usr/local/opt/llvm@7/lib -Wl,-rpath,/usr/local/opt/llvm@7/lib'
```
