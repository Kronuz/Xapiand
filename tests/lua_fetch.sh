#!/bin/sh
# Fetches Lua as CMakeLists.txt does (lua/lua by commit, shallow, limits patch), then runs the drift
# check on it: it must pass on the pin and fail on a wrong commit.
# Usage: lua_fetch.sh SOURCE_DIR [WORK_DIR]   (WORK_DIR defaults to a fresh temporary directory)
set -eu
src=$1 work=${2:-$(mktemp -d "${TMPDIR:-/tmp}/lua_fetch.XXXXXX")}
echo "lua fetch: source $src, work $work"
mkdir -p "$work/p"
cat > "$work/p/CMakeLists.txt" <<CMAKE
cmake_minimum_required(VERSION 3.24)
project(lua_fetch NONE)
include(FetchContent)
include("$src/contrib/lua/lua-version.cmake")
FetchContent_Declare(lua
	GIT_REPOSITORY https://github.com/lua/lua.git
	GIT_TAG \${XAPIAND_LUA_COMMIT}
	GIT_SHALLOW TRUE
	PATCH_COMMAND sh "$src/contrib/lua/apply-patch.sh" "$src/contrib/lua/xapiand-limits.patch")
FetchContent_MakeAvailable(lua)
message(STATUS "LUA_SRC=\${lua_SOURCE_DIR}")
CMAKE
cfg() { cmake -G Ninja -S "$work/p" -B "$1" -DFETCHCONTENT_BASE_DIR="$work/deps" > "$work/cfg.log" 2>&1 || { cat "$work/cfg.log"; exit 1; }; cat "$work/cfg.log"; }
cfg "$work/b"
echo "lua fetch: second configure over the same deps (the patch is already applied)"
cfg "$work/b2"
lua=$(sed -n 's/.*LUA_SRC=//p' "$work/cfg.log")
drift() { cmake "$@" -DLUA_SRC="$lua" -DPATCH="$src/contrib/lua/xapiand-limits.patch" \
	-DVERSION_FILE="$src/contrib/lua/lua-version.cmake" -DWORK="$work/drift" -P "$src/tests/lua_patch_drift.cmake"; }
echo "lua fetch: drift check on the pin"
drift
echo "lua fetch: drift check on a wrong commit (must fail)"
if drift -DLUA_COMMIT=0000000000000000000000000000000000000000; then echo "wrong pin was accepted"; exit 1; fi
echo "lua fetch: ok"
