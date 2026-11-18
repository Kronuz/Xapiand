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

# The Lua release Xapiand builds (github.com/lua/lua tag v5.4.7, pinned by commit) and the hash of
# xapiand-limits.patch, which carries every change to its sources. CMakeLists.txt fetches with these and
# tests/lua_patch_drift.cmake checks them, so a changed Lua fails the build's tests.
set(XAPIAND_LUA_COMMIT "1ab3208a1fceb12fca8f24ba57d6e13c5bff15e3")
set(XAPIAND_LUA_PATCH_SHA256 "9426b7b87f6a3960df64cdb4e633fdd2a9b15566c20b895930becf4a1f48b022")
