#!/bin/sh

set -e

XAPIAND_USER=${XAPIAND_USER:-xapiand}
XAPIAND_DATABASE=${XAPIAND_DATABASE:-/var/run/xapiand}

# if the first argument starts with '-', prepend xapiand
if [ "${1:0:1}" = '-' ]; then
	set -- xapiand "$@"
fi

# allow the container to be started with `--user`
if [ "$1" = 'xapiand' ] && [ "$XAPIAND_USER" != '0' ]; then
	XAPIAND_USER="$(id -u)"
fi

if [ ! -d "$XAPIAND_DATABASE" ]; then
	mkdir -p "$XAPIAND_DATABASE"
	chown -R "$XAPIAND_USER" "$XAPIAND_DATABASE" 2>/dev/null || :
	chmod 700 "$XAPIAND_DATABASE" 2>/dev/null || :
fi

exec "$@" --uid="$XAPIAND_USER" --database="$XAPIAND_DATABASE"
