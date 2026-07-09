#!/usr/bin/env bash
#
# ci_status.sh -- one-glance CI status for a ref (tag, commit, or branch).
#
# Consolidates EVERY GitHub Actions run whose head commit is this ref (CI, FreeBSD,
# Release, ...) and every job within them (including matrix legs -- Docker amd64/
# arm64, RPM x86_64/aarch64, ASan/TSan) into one tree with a clear pass/fail, then
# lists the workflows that did NOT run for this ref (e.g. manual bottles/macOS).
#
# Usage:  harness/ci_status.sh [ref]        # ref defaults to the exact tag on HEAD, else HEAD
#         harness/ci_status.sh v1.0.0-alpha.2
#
set -u
cd "$(git rev-parse --show-toplevel)" || exit 1
command -v gh >/dev/null || { echo "needs the gh CLI"; exit 1; }

REF="${1:-$(git describe --tags --exact-match HEAD 2>/dev/null || git rev-parse HEAD)}"
SHA="$(git rev-parse "${REF}^{commit}" 2>/dev/null)" || { echo "unknown ref: $REF"; exit 1; }
SHORT="$(git rev-parse --short "$SHA")"

icon() { case "$1" in success) printf '✓';; failure|cancelled|timed_out) printf '✗';; *) [ "$2" = completed ] && printf '?' || printf '⏳';; esac; }

echo "── CI status for ${REF} (${SHORT}) ──"
echo

runs="$(gh run list -L 80 --json databaseId,workflowName,status,conclusion,headSha \
	--jq "[.[] | select(.headSha==\"$SHA\")] | sort_by(.workflowName) | .[] | \"\(.databaseId)\t\(.workflowName)\t\(.status)\t\(.conclusion)\"")"

if [ -z "$runs" ]; then
	echo "  (no workflow runs found for this commit yet)"
else
	printf '%s\n' "$runs" | while IFS=$'\t' read -r id wf status concl; do
		printf '%s  %s  (%s)\n' "$(icon "$concl" "$status")" "$wf" "${concl:-$status}"
		gh run view "$id" --json jobs \
			--jq '.jobs | sort_by(.name) | .[] | "\(.name)\t\(.conclusion)\t\(.status)"' 2>/dev/null \
		| while IFS=$'\t' read -r jn jc js; do
			printf '     %s  %s\n' "$(icon "$jc" "$js")" "$jn"
		done
	done
fi

echo
echo "── did NOT run for ${SHORT} ──"
ran="$(printf '%s\n' "$runs" | cut -f2 | sort -u)"
for f in .github/workflows/*.yml; do
	wf="$(awk -F': *' '/^name:/{print $2; exit}' "$f")"; [ -z "$wf" ] && wf="$(basename "$f")"
	printf '%s\n' "$ran" | grep -qxF "$wf" || printf '   ·  %-22s (%s)\n' "$wf" "$(basename "$f")"
done
