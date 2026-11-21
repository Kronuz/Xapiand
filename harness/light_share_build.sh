#!/usr/bin/env bash
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

# light_share_build.sh DIR [PATCH]
#
# Builds xapiand (RelWithDebInfo, frame pointers) into DIR with Kronuz/task at Xapiand's pin plus PATCH
# (default harness/light_share_task.patch, the heavy-share prototype; "none" builds the pin unpatched).
set -eu
DIR=${1:?DIR}; PATCH=${2:-harness/light_share_task.patch}
PIN=$(sed -n 's|.*Kronuz/task.git GIT_TAG \([0-9a-f]*\).*|\1|p' CMakeLists.txt)
TASK="$PWD/.deps-light-share/task-$(basename "$DIR")"
[ -d "$TASK/.git" ] || git clone -q https://github.com/Kronuz/task.git "$TASK"
git -C "$TASK" checkout -q -f "$PIN"
[ "$PATCH" = none ] || git -C "$TASK" apply "$PWD/$PATCH"
echo "task $PIN + $PATCH"
cmake -G Ninja -S . -B "$DIR" -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF \
	-DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -DFETCHCONTENT_SOURCE_DIR_TASK="$TASK" > "$DIR.cmake.log" 2>&1 || { tail -30 "$DIR.cmake.log"; exit 3; }
cmake --build "$DIR" -j4 --target xapiand > "$DIR.build.log" 2>&1 || { grep -m20 -B2 -A8 "error" "$DIR.build.log"; exit 3; }
echo "built $DIR/bin/xapiand at $(git rev-parse --short HEAD)"
