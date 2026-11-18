#!/bin/sh
# Fetches Lua as CMakeLists.txt does (lua/lua by commit, shallow, limits patch), then runs the drift
# check on it: it must pass on the pin and fail on a wrong commit.
# Usage: lua_fetch.sh SOURCE_DIR WORK_DIR
set -eu
src=$1 work=$2
rm -rf "$work"; mkdir -p "$work/p"
cat > "$work/p/CMakeLists.txt" <<CMAKE
cmake_minimum_required(VERSION 3.24)
project(lua_fetch NONE)
include(FetchContent)
include("$src/contrib/lua/lua-version.cmake")
FetchContent_Declare(lua
	GIT_REPOSITORY https://github.com/lua/lua.git
	GIT_TAG \${XAPIAND_LUA_COMMIT}
	GIT_SHALLOW TRUE
	PATCH_COMMAND patch -p1 -i "$src/contrib/lua/xapiand-limits.patch")
FetchContent_MakeAvailable(lua)
message(STATUS "LUA_SRC=\${lua_SOURCE_DIR}")
CMAKE
cmake -S "$work/p" -B "$work/b" > "$work/cfg.log" 2>&1 || { cat "$work/cfg.log"; exit 1; }
lua=$(sed -n 's/.*LUA_SRC=//p' "$work/cfg.log")
drift() { cmake "$@" -DLUA_SRC="$lua" -DPATCH="$src/contrib/lua/xapiand-limits.patch" \
	-DVERSION_FILE="$src/contrib/lua/lua-version.cmake" -DWORK="$work/drift" -P "$src/tests/lua_patch_drift.cmake"; }
drift
if drift -DLUA_COMMIT=0000000000000000000000000000000000000000; then echo "wrong pin was accepted"; exit 1; fi
echo "lua fetch: ok"
