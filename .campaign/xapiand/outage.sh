#!/bin/sh
# Queues the machine work xapiand's next rounds need during the quota outage. Idempotent:
# verify and rerun reuse identical queued or passed jobs; a push already up to date is a no-op.
cd "$(dirname "$0")/../.." || exit 1
OUTAGE_NAME=xapiand . "$HOME/.scratch/campaign/outage/lib.sh"
export GH_TOKEN="$(gh auth token --user Kronuz)"

# Branches that need CI at their heads (code worktree, then the journal worktree).
for q in journal-replaces-wal rocksdb-parity strata-journal-behind consensus-recover-gate track-c; do
	for wt in "$(campaign get "$q" worktree 2>/dev/null)" "$(campaign get "$q" journal 2>/dev/null | sed 's#/\.campaign/.*##')"; do
		[ -d "$wt" ] && git -C "$wt" push -q 2>&1 | grep -v '^remote:'
	done
done

# Each quest's checks at its head.
for q in journal-replaces-wal rocksdb-parity strata-journal-behind consensus-recover-gate track-c; do
	campaign verify "$q" 2>&1 | grep -vE 'atexit|Traceback|File "|pipe.close|BrokenPipe'
done

# Evidence that ran other code than the heads (reruns at the current head).
for j in 261125-004551-0bc60c 261125-033043-326a1d 261125-033134-b6ba15; do
	step "sjb-rerun-$j" -- campaign job rerun "$j"
done

campaign job list --queued 2>/dev/null | head -40 || true
