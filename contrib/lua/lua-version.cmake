# The Lua release Xapiand builds, the hash of its tarball, and the hash of lstrlib.c once
# lstrlib-step-budget.patch is applied. CMakeLists.txt fetches with these and
# tests/lua_patch_drift.cmake checks them, so a changed Lua fails the build's tests.
set(XAPIAND_LUA_URL "https://www.lua.org/ftp/lua-5.4.7.tar.gz")
set(XAPIAND_LUA_TARBALL_SHA256 "9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30")
set(XAPIAND_LUA_LSTRLIB_PRISTINE_SHA256 "3257cd857653560773947d70b65c8b8bf8ef43f920ed449587fa662224d859e1")
set(XAPIAND_LUA_LSTRLIB_PATCHED_SHA256 "376531bda7b37b8ec5865786e3c670bb9f24539891d2cdef9bd8cd5d4160a350")
