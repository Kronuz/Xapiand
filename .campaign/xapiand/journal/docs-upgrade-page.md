# docs-upgrade-page: journal

State: waiting on jobs 261109-195610-7eb701 261109-195616-e08052
Quest: .campaign/xapiand/quests/docs-upgrade-page.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rebased onto master (includes cli-restore-records, Xapiand:b4acd8419) at 5e8f9a629. A2 v4 (acc42a) had failed
on path 1 only (the CLI `--restore` defect; path 2, HTTP, matched 3000/3000, and the new binary exits 65 on the
old directory). Resubmitted the same command at the rebased head as A2 v5, 261109-195610-7eb701 (slot a2-roundtrip,
75 min). E2E at the rebased head: 261109-195616-e08052 (`campaign verify`; master's `tested_docs` now covers
`docs/src/content/docs/**`, so the old e2e evidence no longer counts).

## Next

1. When 7eb701 ends: path 1 (CLI) and path 2 (HTTP) both OK is A2's evidence; if path 1 still fails, diagnose
   with the job log (`.a2/logs/`), report to the lead (the fix is outside this quest's file set).
2. When e08052 ends: A3 and A4 evidence. Then State "ready for review".

## Blocked on

None. (Earlier: the CLI `--restore` defect, now fixed by cli-restore-records.)

Earlier findings kept: `DUMP` with `Accept: application/x-ndjson` answers 406 (backups.md corrected);
the refusal anchor `#upgrading-from-100-alpha9` is carried by an explicit `<a id>` (site build job
261109-181744-d76928); the changelog's [Unreleased] carries the `--restore` entry moved from cli-restore-records.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/src/content/docs/upgrade.md` |
| A3 | job 261109-152541-04dbf7 (e2e, exit 0 at 4f289be1c); the page covers audit rows and the D44 encoding; the reviewer reads the rest |
| A4 | job 261109-152541-04dbf7 (e2e, exit 0 at 4f289be1c; reused by `campaign verify` at c14d6eaac); refusal anchor in the built site: job 261109-181744-d76928 |

History: A2 job 261109-153551-16cd29 exited 65 after "new restore exit 0" (dump and restore sha256 matched): my script's
`rc=$?` line ran under `set -e`, so the new binary's expected exit 65 on the old directory ended the script. Fixed with
`|| rc=$?`; resubmitted as 261109-162803-b8445a (slot a2-roundtrip, 90 min), which failed: old docs 0 new docs 0
(406 to the NDJSON DUMP, and the CLI `--restore` defect above). v3 261109-174137-865a4a failed before running:
the old build's CMake cache named the previous job's ref checkout (`.../261109-162803-b8445a/ref-old`), and each
job gets its own. v4 symlinks the ref to `.a2/src-old` and drops a cache that names another source; v4 is
261109-181209-acc42a.

## Result

<When ready for review.>

## Rulings I made

1. File name: kept `upgrade.md` (not `upgrading.md`): the sidebar (`docs/src/lib/sidebar.mjs:420`), the changelog and
   the Xapian-data refusal message (`src/xapian_data_guard.h:104`) link `/upgrade`. A1's path is therefore `upgrade.md`.
2. Kept the 0.40.0 content (ids, rolling back) in the same page; the alpha.9/alpha.10 section leads.
3. Left out the audit's nested-index refusal (#21): the changelog now says indexes nest (the owner reversed it).
4. Left out limits for the not-yet-landed `--search-walk-queue` rows and #36-37 details beyond shutdown bound.
5. A2's harness lives in the job command (spec.json of acc42a; drafts in this worktree's ignored `.scratch/a2/`),
   not in `harness/`, which is outside this quest's file set.
