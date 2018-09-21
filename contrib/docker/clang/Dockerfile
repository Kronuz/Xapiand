################################################################################
#   ____ _
#  / ___| | __ _ _ __   __ _
# | |   | |/ _` | '_ \ / _` |
# | |___| | (_| | | | | (_| |
#  \____|_|\__,_|_| |_|\__, |
#                      |___/
#
# Build using:
# docker build -t dubalu/clang:latest contrib/docker/clang
# docker push dubalu/clang
#
# Or using a cloud container with 64 CPUs:
# gcloud container --project=dubalu-io-cloud clusters create clang-builder --enable-ip-alias --enable-autoupgrade --cluster-version=latest --num-nodes=1 --machine-type=n1-highcpu-64
# kubectl run -it clang-builder --image=alpine:3.8 -- sh

# Temporary workaround for there not being an llvm6 apk yet
# Open PR for llvm6 package: https://github.com/alpinelinux/aports/pull/3583
# See https://stackoverflow.com/questions/50258121/building-llvm-6-under-linux-alpine

FROM alpine:3.8

RUN apk update \
  && apk add --no-cache --virtual .build-deps \
    alpine-sdk \
    git \
    diffutils \
    patchelf \
  && adduser -D apk \
  && adduser apk abuild \
  && sudo -iu apk abuild-keygen -a \
  && cp /home/apk/.abuild/*.rsa.pub /etc/apk/keys \
  && sudo -iu apk git clone --depth=1 -b pr-llvm-6 https://github.com/Kronuz/aports \
  \
  # For faster builds in multi-processor boxes:
  # && sudo -iu apk sh -xec 'cd aports/main/llvm6; sed -i "s@\bmake\b@make -j96@g" APKBUILD' \
  # && sudo -iu apk sh -xec 'cd aports/main/clang; sed -i "s@\bmake\b@make -j96@g" APKBUILD' \
  # && sudo -iu apk sh -xec 'cd aports/main/compiler-rt; sed -i "s@\bmake\b@make -j96@g" APKBUILD' \
  # && sudo -iu apk sh -xec 'cd aports/community/lld; sed -i "s@\bmake\b@make -j96@g" APKBUILD' \
  # && sudo -iu apk sh -xec 'cd aports/community/llvm-libunwind; sed -i "s@\bmake\b@make -j96@g" APKBUILD' \
  # && sudo -iu apk sh -xec 'cd aports/testing/libc++; sed -i "s@\bmake\b@make -j96@g" APKBUILD' \
  \
  # First pass.
  # Build the needed components using gcc (libc++ is build last, using the built clang):
  && sudo -iu apk sh -xec 'cd aports/main/llvm6; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/llvm6-*.apk \
  && sudo -iu apk sh -xec 'cd aports/main/clang; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/clang-*.apk \
  && sudo -iu apk sh -xec 'cd aports/community/lld; abuild -r' \
  && apk add /home/apk/packages/community/$(uname -m)/lld-*.apk \
  && sudo -iu apk sh -xec 'cd aports/community/llvm-libunwind; abuild -r' \
  && apk add /home/apk/packages/community/$(uname -m)/llvm-libunwind-*.apk \
  && sudo -iu apk sh -xec 'cd aports/testing/libc++; abuild -r' \
  && apk add /home/apk/packages/testing/$(uname -m)/libc++-*.apk \
  && sudo -iu apk sh -xec 'cd aports/main/compiler-rt; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/compiler-rt-*.apk \
  \
  # Second pass.
  # Build and install a llvm6 and clang which compiles using lld, libc++ and compiler-rt:
  # (but first patch rpath for current clang and lld so they keep working during the process)
  && rm /usr/lib/libunwind.so* \
  && sudo -iu apk cp -R /usr/lib lib1 \
  && patchelf --set-rpath '/home/apk/lib1:/home/apk/lib1/llvm6/lib:$ORIGIN/../lib:/usr/lib/llvm6/lib' /usr/bin/clang \
  && patchelf --set-rpath '/home/apk/lib1:/home/apk/lib1/llvm6/lib:$ORIGIN/../lib:/usr/lib/llvm6/lib' /usr/bin/lld \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; cd aports/main/llvm6; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/llvm6-*.apk \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; export CLANG_DEFAULT_LINKER="lld"; export CLANG_DEFAULT_CXX_STDLIB="libc++"; export CLANG_DEFAULT_RTLIB="compiler-rt"; cd aports/main/clang; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/clang-*.apk \
  \
  # Third pass.
  # Rebuild and install everything using new clang with libc++ and compiler-rt (kill all gcc dependencies):
  # (but first patch rpath for current clang so it keeps working during the process)
  && sudo -iu apk cp -R /usr/lib lib2 \
  && patchelf --set-rpath '/home/apk/lib2:/home/apk/lib2/llvm6/lib:$ORIGIN/../lib:/usr/lib/llvm6/lib' /usr/bin/clang \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; cd aports/main/llvm6; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/llvm6-*.apk \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; export CLANG_DEFAULT_LINKER="lld"; export CLANG_DEFAULT_CXX_STDLIB="libc++"; export CLANG_DEFAULT_RTLIB="compiler-rt"; cd aports/main/clang; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/clang-*.apk \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; cd aports/main/compiler-rt; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/main/$(uname -m)/compiler-rt-*.apk \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; cd aports/community/lld; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/community/$(uname -m)/lld-*.apk \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; cd aports/community/llvm-libunwind; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/community/$(uname -m)/llvm-libunwind-*.apk \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; export LIBCXX_USE_COMPILER_RT="ON"; cd aports/testing/libc++; abuild cleanpkg; abuild -r' \
  && apk add /home/apk/packages/testing/$(uname -m)/libc++-*.apk \
  && sudo -iu apk sh -xec 'export CC="clang"; export CXX="clang++"; cd aports/testing/lldb; abuild -r' \
  && apk add /home/apk/packages/testing/$(uname -m)/lldb-*.apk \
  \
  # Save .apk packages
  && mkdir /packages \
  && cp /home/apk/.abuild/apk-*.rsa.pub /packages \
  && find /home/apk/packages -name '*.apk' -exec mv {} /packages \; \
  \
  # Cleanup:
  && deluser --remove-home apk \
  && rm -rf /var/cache/apk/APKINDEX* \
  && apk del .build-deps

FROM alpine:3.8

COPY --from=0 /packages /packages

RUN cp /packages/apk-*.rsa.pub /etc/apk/keys \
  && apk add /packages/libc++-6.0.1-r0.apk \
    /packages/llvm-libunwind-6.0.1-r0.apk \
  && apk add --no-cache --virtual build-clang \
    binutils \
    musl-dev \
    /packages/llvm6-libs-6.0.1-r0.apk \
    /packages/compiler-rt-6.0.1-r0.apk \
    /packages/llvm-libunwind-dev-6.0.1-r0.apk \
    /packages/clang-6.0.1-r0.apk \
    /packages/clang-libs-6.0.1-r0.apk \
    /packages/clang-dev-6.0.1-r0.apk \
    /packages/lld-6.0.1-r0.apk \
    /packages/lld-libs-6.0.1-r0.apk \
    /packages/libc++-dev-6.0.1-r0.apk \
    /packages/lldb-6.0.1-r0.apk \
    /packages/lldb-dev-6.0.1-r0.apk \
    /packages/py2-lldb-6.0.1-r0.apk \
  && rm -rf /packages

# The following are intentionally left out of the final image:
# packages/clang-doc-6.0.1-r0.apk
# packages/llvm6-6.0.1-r0.apk
# packages/clang-static-6.0.1-r0.apk
# packages/clang-analyzer-6.0.1-r0.apk
# packages/lld-dev-6.0.1-r0.apk
# packages/llvm6-test-utils-6.0.1-r0.apk
# packages/llvm6-dev-6.0.1-r0.apk
# packages/llvm6-static-6.0.1-r0.apk
