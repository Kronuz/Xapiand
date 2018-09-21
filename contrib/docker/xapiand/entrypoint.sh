#!/bin/sh

set -e

# Setup user/group (allow the container to be started with `--user`):
UID="${UID:-$(id -u -n)}"
if [ "$UID" == 'root' ]; then
	UID='xapiand'
fi

GID="${GID:-$(id -g -n)}"
if [ "$GID" == 'root' ]; then
	GID='xapiand'
fi

# if the first argument starts with '-', prepend xapiand
if [ "${1:0:1}" = '-' ] || [ -z "$1" ]; then
	set -- xapiand "$@"
fi

# Add defaults to command
if [ "$1" = 'xapiand' ]; then
	XAPIAND_DATABASE="${XAPIAND_DATABASE:-/var/db/xapiand}"
	XAPIAND_CLUSTER="${XAPIAND_CLUSTER:$HOSTNAME}"

	if [ ! -d "$XAPIAND_DATABASE" ]; then
		mkdir -p "$XAPIAND_DATABASE"
		chown -R "$UID":$GID "$XAPIAND_DATABASE" 2>/dev/null || :
		chmod 700 "$XAPIAND_DATABASE" 2>/dev/null || :
	fi

	set -- "$@" \
		--uid="$UID" \
		--gid="$GID" \
		--cluster="$XAPIAND_CLUSTER" \
		--database="$XAPIAND_DATABASE"
fi

exec "$@"
