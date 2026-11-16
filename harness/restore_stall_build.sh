#!/bin/bash
# Builds the restore-stall A/B's two binaries, both with the reactor's lag probe patch (inert unless
# XAPIAND_LAG_SPIN is set): this checkout's into build-rs/ and BASE's (a checkout with the same pins) into
# build-rs-base/. Usage, in the checkout's root, under `quiet light`: harness/restore_stall_build.sh BASE_DIR
set -u
BASE=$1
DEPS="$PWD/.deps"
PATCH="$PWD/harness/restore_stall_reactor_lag.patch"
FLAGS=(-DFETCHCONTENT_BASE_DIR="$DEPS" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer)
fail() {
	tail -30 "$@"
	exit 2
}
cmake -S . -B build-rs "${FLAGS[@]}" > cfg.log 2>&1 || fail cfg.log
cmake -S "$BASE" -B build-rs-base "${FLAGS[@]}" > cfg-base.log 2>&1 || fail cfg-base.log
git -C "$DEPS/reactor-src" apply --reverse --check "$PATCH" 2> /dev/null || git -C "$DEPS/reactor-src" apply "$PATCH" || exit 2
cmake --build build-rs -j4 --target xapiand > build.log 2>&1 || fail build.log
cmake --build build-rs-base -j4 --target xapiand > build-base.log 2>&1 || fail build-base.log
git -C "$DEPS/reactor-src" apply --reverse --check "$PATCH" || exit 2   # both built with the probe
ls -la build-rs/bin/xapiand build-rs-base/bin/xapiand
