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
