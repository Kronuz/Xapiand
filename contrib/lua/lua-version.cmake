# The Lua release Xapiand builds, the hash of its tarball, and the hash of xapiand-limits.patch, which
# carries every change to its sources. CMakeLists.txt fetches with these and tests/lua_patch_drift.cmake
# checks them, so a changed Lua fails the build's tests.
set(XAPIAND_LUA_URL "https://www.lua.org/ftp/lua-5.4.7.tar.gz")
set(XAPIAND_LUA_TARBALL_SHA256 "9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30")
set(XAPIAND_LUA_PATCH_SHA256 "cbee79a9874b15525d4c55986b265465056d90ce7686a2946cab7e1a57bf4df4")
