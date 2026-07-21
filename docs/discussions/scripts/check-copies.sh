#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -eq 0 ]; then
  printf 'usage: %s <other-discussions-dir> [...]\n' "$0" >&2
  exit 2
fi

targets=()
for target in "$@"; do
  case "$target" in
    /*) targets+=("$target") ;;
    *) targets+=("$PWD/$target") ;;
  esac
done

cd "$(dirname "$0")/.."

for target in "${targets[@]}"; do
  [ -d "$target" ] || { printf 'missing directory: %s\n' "$target" >&2; exit 1; }
  changes=$(rsync -nrc --delete --itemize-changes \
    --exclude '.DS_Store' \
    --exclude 'worker/node_modules/' \
    --exclude 'worker/.wrangler/' \
    --exclude 'worker/.dev.vars' \
    --exclude 'worker/secrets.sh' \
    --exclude 'worker/wrangler.toml' \
    --include 'worker/tenant-config.example.json' \
    --exclude 'worker/tenant-config.*.json' \
    ./ "$target/")
  if [ -n "$changes" ]; then
    printf 'Discussions copy differs: %s\n%s\n' "$target" "$changes" >&2
    exit 1
  fi
  printf 'Discussions copy matches: %s\n' "$target"
done
