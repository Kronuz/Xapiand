#!/usr/bin/env bash
# async_check.sh [--strict [--owed]] [--list] [--json] [--extern NAME=DIR]...
#
# Phase 7c D14: classify every blocking primitive on a reactor-reachable path. See async_check.py.
PY=/usr/local/bin/python3
[ -x "$PY" ] || PY=python3
exec "$PY" -I "$(dirname "$0")/async_check.py" "$@"
