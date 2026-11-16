#!/usr/bin/env bash
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

#
# ci_status.sh -- one-glance CI status for a ref (tag, commit hash, or branch).
#
# Consolidates EVERY GitHub Actions run whose head commit is this ref (CI, FreeBSD,
# Release, Bottles, ...) and every job within them (including matrix legs -- Docker
# amd64/arm64, RPM x86_64/aarch64, ASan/TSan) into one tree with a clear pass/fail,
# then lists the workflows that did NOT run for this ref (e.g. manual bottles/macOS).
#
# Usage:  harness/ci_status.sh [ref]
#   ref may be a tag, a commit hash (short or full), or a branch;
#   defaults to the exact tag on HEAD, else HEAD.
#
#   harness/ci_status.sh                 # current tag/HEAD
#   harness/ci_status.sh v1.0.0-alpha.2  # a tag
#   harness/ci_status.sh 4cd9f3d17       # a specific commit
#
set -u

if [ "${1:-}" = "-h" ] || [ "${1:-}" = "--help" ]; then
	sed -n '3,17p' "$0" | sed 's/^# \{0,1\}//'
	exit 0
fi

cd "$(git rev-parse --show-toplevel)" || exit 1
command -v gh >/dev/null || { echo "needs the gh CLI"; exit 1; }

REF="${1:-$(git describe --tags --exact-match HEAD 2>/dev/null || git rev-parse HEAD)}"
SHA="$(git rev-parse "${REF}^{commit}" 2>/dev/null)" || { echo "unknown ref: $REF"; exit 1; }
SHORT="$(git rev-parse --short "$SHA")"

# Label: a raw hash collapses to its short form; a tag/branch keeps its name.
case "$SHA" in
	"$REF"*) LABEL="$SHORT" ;;
	*)       LABEL="$REF ($SHORT)" ;;
esac

icon() { case "$1" in success) printf '✅';; failure|cancelled|timed_out) printf '❌';; *) [ "$2" = completed ] && printf '❔' || printf '⏳';; esac; }

echo "── CI status for ${LABEL} ──"
echo

runs="$(gh run list -L 200 --json databaseId,workflowName,status,conclusion,headSha \
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
