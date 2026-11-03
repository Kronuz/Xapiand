#!/usr/bin/env bash
# profile_light.sh BIN OUT [--secs 10] [--docs 20000] [--label NAME] [--port 18991]
# profile_light.sh --compare PRE.json POST.json
#
# Phase 7c K6 / T11: CPU per light request by group (offload hop, scheduler, engine, HTTP) from
# `sample` frames (macOS) or `perf` (Linux), and the hop's wall time from the http trace/hop stamps. See profile_light.py.
. "$(dirname "$0")/env.sh"
exec "$PY" -I "$(dirname "$0")/profile_light.py" "$@"
