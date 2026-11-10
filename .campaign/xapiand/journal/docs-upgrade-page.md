# docs-upgrade-page: journal

State: waiting on jobs 261109-174137-865a4a
Quest: .campaign/xapiand/quests/docs-upgrade-page.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A2 v3 (261109-174137-865a4a, light, 75 min) runs the round trip fail-loud: HTTP status checked on every call,
one log per server under `.a2/logs/`, COUNT awaited at 3000 on the old node before dumping, DUMP as JSON.
It restores the old CLI dump twice: path 1 the page's `--restore` (expected to fail on the defect below),
path 2 `RESTORE` over HTTP of the same msgpack dump; compares each against the old node's DUMP.

Lead's two problems, answered:
1. Empty old.dump: harness. `DUMP` with `Accept: application/x-ndjson` answers 406 with an empty body on both
   binaries (probe 261109-173432-6270fa: old and new both 406; the old node held 3000 documents, COUNT 3000
   right after RESTORE). `-s` without a status check hid it. DUMP serves JSON, YAML, MessagePack only
   (`src/database/data.h:99`, `msgpack_serializers`). v3 asks for JSON.
2. Replica inconsistency: a product defect in the head binary's CLI `--restore` into an index the data
   directory has no settings for; not solo/name (head rejects `--solo`, exit 109; restore and server both
   used `--name new`, no `--solo`). Reproduction 261109-174137-c5db06 (head binary only, 20 documents, same
   `--name n` everywhere): HTTP RESTORE then restart: COUNT 20; CLI `--restore` into an empty dir then serve:
   COUNT 500 "Inconsistency in number of replicas configured for 'xapiand://n/.xapiand/indexes'";
   with `--shards 1`: COUNT 0 (20 restored documents invisible); index created first (`PUT /rt/`, stop),
   then CLI `--restore`: COUNT 20; HTTP RESTORE of the CLI's msgpack dump: COUNT 20, also after restart.
   Probe 6270fa: after CLI `--restore`, `.xapiand/indexes` lacks the `rt/.__N` shard records an
   HTTP-created index has (the restore wrote shard dirs `rt/.__1..5` and `.xapiand/indexes/.__1..5`).

## Next

1. When 865a4a ends: record path 2 (HTTP) result as A2's evidence for the HTTP route; path 1 stays failed
   until the defect is fixed, then rerun the same job command (in its spec.json) at the fixed head.
2. A3's reviewer read; A4 stands (e2e 261109-152541-04dbf7).

## Blocked on

lead: the CLI `--restore` defect (reproduction 261109-174137-c5db06, outside this docs quest's file set).
The page's step 5 (`xapiand -D <new> --restore=... --in=...` into a new, empty directory) is the failing case.
Options: (a) file and fix the defect, keep the page as written, rerun A2 path 1 after (recommended: the
release docs should not carry a workaround for a bug); (b) the page tells users to create each index on the
new node first (start, `PUT /<index>/`, stop) or to restore over HTTP (under 16 MiB), and A2 proves that route.

Also for the lead's hunt list (pre-existing, not blocking): `docs/src/content/docs/reference-guide/indexes/backups.md`
("MessagePack (or NDJSON)") says `DUMP` with `Accept: application/x-ndjson` works; both binaries answer 406.
Either the doc drops NDJSON for DUMP (in this file set; I can do it on a ruling) or DUMP gains NDJSON.
(Earlier lead rulings: A2 built in-job; numbers bullet confirmed against PLAN D44; the alpha.10 refusal
text is on the lead's hunt list.)

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/src/content/docs/upgrade.md` |
| A3 | job 261109-152541-04dbf7 (e2e, exit 0 at 4f289be1c); the page covers audit rows and the D44 encoding; the reviewer reads the rest |
| A4 | job 261109-152541-04dbf7 (e2e, exit 0 at 4f289be1c) |

History: A2 job 261109-153551-16cd29 exited 65 after "new restore exit 0" (dump and restore sha256 matched): my script's
`rc=$?` line ran under `set -e`, so the new binary's expected exit 65 on the old directory ended the script. Fixed with
`|| rc=$?`; resubmitted as 261109-162803-b8445a (slot a2-roundtrip, 90 min), which failed: old docs 0 new docs 0
(406 to the NDJSON DUMP, and the CLI `--restore` defect above). v3 is 261109-174137-865a4a.

## Result

<When ready for review.>

## Rulings I made

1. File name: kept `upgrade.md` (not `upgrading.md`): the sidebar (`docs/src/lib/sidebar.mjs:420`), the changelog and
   the Xapian-data refusal message (`src/xapian_data_guard.h:104`) link `/upgrade`. A1's path is therefore `upgrade.md`.
2. Kept the 0.40.0 content (ids, rolling back) in the same page; the alpha.9/alpha.10 section leads.
3. Left out the audit's nested-index refusal (#21): the changelog now says indexes nest (the owner reversed it).
4. Left out limits for the not-yet-landed `--search-walk-queue` rows and #36-37 details beyond shutdown bound.
5. A2's harness lives in the job command (spec.json of 865a4a; drafts in this worktree's ignored `.scratch/a2/`),
   not in `harness/`, which is outside this quest's file set.
