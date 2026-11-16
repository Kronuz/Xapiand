# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# The Lua release Xapiand builds, the hash of its tarball, and the hash of xapiand-limits.patch, which
# carries every change to its sources. CMakeLists.txt fetches with these and tests/lua_patch_drift.cmake
# checks them, so a changed Lua fails the build's tests.
# The URL list tries lua.org, then its official PUC-Rio mirror, which serves the same tarball.
set(XAPIAND_LUA_URL "https://www.lua.org/ftp/lua-5.4.7.tar.gz" "https://www.tecgraf.puc-rio.br/lua/mirror/ftp/lua-5.4.7.tar.gz")
set(XAPIAND_LUA_TARBALL_SHA256 "9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30")
set(XAPIAND_LUA_PATCH_SHA256 "cbee79a9874b15525d4c55986b265465056d90ce7686a2946cab7e1a57bf4df4")
