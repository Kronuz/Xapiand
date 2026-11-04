#!/usr/bin/env bash
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
# Copyright © 2015-2020 Dubalu LLC
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

set -e

# if the first argument starts with '-', prepend "xapiand"
if [ "${1:0:1}" = '-' ] || [ -z "$1" ]; then
	set -- xapiand "$@"
fi

if [ "$1" = 'xapiand' ]; then

	# Normalize variables and allow the container to be started with `--user`
	XAPIAND_UID="${XAPIAND_UID:-$UID}"
	XAPIAND_GID="${XAPIAND_GID:-$GID}"
	XAPIAND_NAME="${XAPIAND_NAME:-$HOSTNAME}"
	XAPIAND_DATABASE="${XAPIAND_DATABASE:-/var/db/xapiand}"

	# Setup user
	XAPIAND_UID="${XAPIAND_UID:-$(id -u -n)}"
	if [ "$XAPIAND_UID" == 'root' ]; then
		XAPIAND_UID='xapiand'
	fi

	# Setup group
	XAPIAND_GID="${XAPIAND_GID:-$(id -g -n)}"
	if [ "$XAPIAND_GID" == 'root' ]; then
		XAPIAND_GID='xapiand'
	fi

	# Set defaults for command
	UID="$XAPIAND_UID"
	GID="$XAPIAND_GID"
	NAME="$XAPIAND_NAME"
	DATABASE="$XAPIAND_DATABASE"

	# Parse arguments
	for i in "$@"
	do
	case $i in
		--uid)
			unset XAPIAND_UID
			next=UID;;
		--uid=*)
			unset XAPIAND_UID
			UID="${i#*=}";;
		--gid)
			unset XAPIAND_GID
			next=GID;;
		--gid=*)
			unset XAPIAND_GID
			GID="${i#*=}";;
		--name)
			unset XAPIAND_NAME
			next=NAME;;
		--name=*)
			unset XAPIAND_NAME
			NAME="${i#*=}";;
		-D|--database)
			unset XAPIAND_DATABASE
			next=DATABASE;;
		-D=*|--database=*)
			unset XAPIAND_DATABASE
			DATABASE="${i#*=}";;
		*)
			if [ ! -z "$next" ]; then
				eval "$next"="$i"
				unset next
			fi;;
	esac
	done

	# Make missing database directory
	if [ ! -d "$DATABASE" ]; then
		mkdir -p "$DATABASE"
		chown -R "$UID":$GID "$DATABASE" 2>/dev/null || :
		chmod 700 "$DATABASE" 2>/dev/null || :
	fi

	# Add missing arguments
	set -- "$@" \
		"${XAPIAND_UID:+--uid=$XAPIAND_UID}" \
		"${XAPIAND_GID:+--gid=$XAPIAND_GID}" \
		"${XAPIAND_NAME:+--name=$XAPIAND_NAME}" \
		"${XAPIAND_DATABASE:+--database=$XAPIAND_DATABASE}"
fi

exec "$@"
