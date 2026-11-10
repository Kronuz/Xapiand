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

# Sourced by the harness scripts. Sets PY (the python3 the helpers run under) and raises the open-files limit.
#   PYTHON=/path/to/python3 overrides; otherwise /usr/local/bin/python3 when present, else python3 on PATH.
if [ -z "${PY:-}" ]; then
	if [ -n "${PYTHON:-}" ]; then PY=$PYTHON
	elif [ -x /usr/local/bin/python3 ]; then PY=/usr/local/bin/python3
	else PY=$(command -v python3) || { echo "harness: no python3 on PATH (set PYTHON)" >&2; exit 2; }
	fi
fi

# A node run with --tls-max-connections 1024 needs more than the 1024 descriptors a login shell starts with
# (the server refuses to start rather than run out). Raise the soft limit to HARNESS_NOFILE (default 65536), capped at the hard limit.
raise_nofile() {
	local want=${HARNESS_NOFILE:-65536} hard
	hard=$(ulimit -Hn)
	[ "$hard" = unlimited ] || [ "$hard" -ge "$want" ] || want=$hard
	[ "$(ulimit -Sn)" = unlimited ] || [ "$(ulimit -Sn)" -ge "$want" ] || ulimit -Sn "$want" 2>/dev/null \
		|| echo "harness: cannot raise the open-files limit above $(ulimit -Sn); TLS startup needs --tls-max-connections + 64" >&2
	return 0
}
raise_nofile

# free_port prints a TCP port the OS has just handed out, so concurrent harness runs never share one.
free_port() {
	"$PY" -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1])'
}
