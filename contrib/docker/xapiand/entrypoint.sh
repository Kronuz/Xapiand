#!/bin/sh

set -e

# if the first argument starts with '-', prepend "xapiand"
if [ "${1:0:1}" = '-' ] || [ -z "$1" ]; then
	set -- xapiand "$@"
fi

if [ "$1" = 'xapiand' ]; then
	K8S_NAMESPACE_PATH="${K8S_NAMESPACE_PATH:-/var/run/secrets/kubernetes.io/serviceaccount/namespace}"
	NAMESPACE="$(test -f $K8S_NAMESPACE_PATH && cat $K8S_NAMESPACE_PATH)"

	# Normalize variables and allow the container to be started with `--user`
	XAPIAND_UID="${XAPIAND_UID:-$UID}"
	XAPIAND_GID="${XAPIAND_GID:-$GID}"
	XAPIAND_NAME="${XAPIAND_NAME:-$HOSTNAME}"
	XAPIAND_CLUSTER="${XAPIAND_CLUSTER:-$NAMESPACE}"
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
	CLUSTER="$XAPIAND_CLUSTER"
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
		--cluster)
			unset XAPIAND_CLUSTER
			next=CLUSTER;;
		--cluster=*)
			unset XAPIAND_CLUSTER
			CLUSTER="${i#*=}";;
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
		"${XAPIAND_CLUSTER:+--cluster=$XAPIAND_CLUSTER}" \
		"${XAPIAND_DATABASE:+--database=$XAPIAND_DATABASE}"
fi

exec "$@"
