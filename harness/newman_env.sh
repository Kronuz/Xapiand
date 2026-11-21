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

# Sourced by the docs E2E scripts. Puts the newman pinned in harness/package.json (installed locally with
# `npm ci`, never globally) first on PATH, so a newman found earlier on PATH never decides the version.
# Returns 1 when it cannot install it. HARNESS_DIR is set by the sourcing script.
_nm_dir="${HARNESS_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
_nm_want="$(sed -n 's/.*"newman": *"\([^"]*\)".*/\1/p' "$_nm_dir/package.json")"
if [ "$("$_nm_dir/node_modules/.bin/newman" --version 2>/dev/null)" != "$_nm_want" ]; then
	npm ci --prefix "$_nm_dir" --no-audit --no-fund --loglevel=error >&2 \
		&& echo '*' > "$_nm_dir/node_modules/.gitignore" || { echo "cannot install newman $_nm_want (npm ci --prefix $_nm_dir)" >&2; return 1 2>/dev/null || exit 1; }
fi
PATH="$_nm_dir/node_modules/.bin:$PATH"
echo "newman $(newman --version) (pinned in harness/package.json)"
unset _nm_dir _nm_want
