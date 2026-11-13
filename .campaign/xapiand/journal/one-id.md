# one-id: journal

State: ready for review
Quest: .campaign/xapiand/quests/one-id.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done at 8d61b1913 (round 4 green: build-test, seven mutants, A5, CI at 3d968d72c). A durability review runs at
8d61b1913; no code changes meanwhile.

Round 3 (2890b08db): build-test 261113-082922-3f2ebf failed only on async_check (`async_allowlist.tsv:25:
src/database/shard.cc [sync_wait] covers 8 hit(s), the row allows 7`): `open_blocking`'s read of `_id_high`. Kept
(R7) with its own row; `harness/async_check.sh --strict` passes on the worktree (static scan, exit 0).

Round 2 (d174690aa): build-test 261113-081918-7c337f failed only on the id key. Probe lines, test_node.cc:2426:
`id 4294967297: its key names 4311744512, as a float 4311744512` and `id 1099511627779: its key names
1099561959424, as a float 1099561959424`; :1633 `/ids_int_s2/4311744512: GET gives _id 4294967297`. The issued key
equals `Serialise::integer`'s (that check passed); the numeric key encoding itself does not name integers past 2^32
exactly (both integer and float keys), and the GET of 4311744512 finding the document keyed 4294967297 shows two ids
share one key. 2^53 - 1 and ids below 2^32 round trip. Fix (2890b08db): MAX_NUMERIC_ID = 2^32 - 1 (R4 revised);
larger ids hash. The ids test's far integer id moves to 4294967000 (an explicit 2^32 - 1 would leave the counter
nothing to issue) and 4294967297 joins as a hashed id; the stride test checks integer and float keys up to the bound.

Round 1 (622f57d08) failed: build-test 261113-074720-30a477 exit 8, probe 261113-074628-298443; the master build
261113-074808-b39d85 copied `build-rel/xapiand`, which is `build-rel/bin/xapiand`. Diagnosed with probe
261113-081251-9ec85d (prints at 73c5cb00d). Causes and fixes (40f8fb586):

- test_pool counter test, version conflict on f: the blobs' push hands the log a copy of the record taken before
  `finalise_write`, so the logged d and e kept the placeholder `_id` 0, the replay raised nothing, and f was issued
  12 again. Code: `issue_id` writes the id into the record, id slot and key before the push; `finalise_write` only
  stamps the version. (Also fixes a replayed automatic id reading back as `_id` 0.)
- test_document_ids_are_themselves, `/ids_int_s2/4311744512: GET gives _id 4294967297`: `numeric_key` used the
  long double encoding, whose key for 2^32 + 1 is the integer encoding's key for 2^32 + 2^24. Code: `numeric_key`
  is the integer encoding a PUT uses (`Serialise::positive`); a float placeholder keeps the float encoding. The
  stride test now checks the keys of ids up to 2^53 - 1 against `Serialise::integer` and their round trip.
- test_far_ids `CHECK(merged)` on shards 0 and 1: the write buffer held the far ids (no commit reached those
  shards), so `merge_all` saw one segment. Test: `merge_all` commits before listing segments (passes in the probe).
- `next_auto_id(1020, 0, 3)`: 1021 is bucket 1020, shard 0's; my expectation was wrong. Test: 1021, and the wrap
  case from 1021 gives 1024 (passes in the probe).

## Next

1. Fold in the review's findings by id, each fix with a test that fails without it.

## Blocked on

Nothing blocking. For the lead (finding, pre-existing on master's encoding, outside this quest): explicit PUTs of
4294967297 and 4311744512 name one key, so one overwrites the other. The serialisation is GPL code I may not read;
license-closure's replacement could make the key exact to 2^53, and MAX_NUMERIC_ID would follow it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261113-083755-c5e4b4; fails without: 261113-083755-2de9cb |
| A2 | 261113-083755-c5e4b4; fails without: 261113-083758-e58dc8, 261113-083800-510abb, 261113-083802-c6d044, 261113-083805-101e1b |
| A7 | 261113-083755-c5e4b4; fails without: 261113-083807-92969a |
| A6 | 261113-083755-c5e4b4; fails without: 261113-083809-734db4 |
| A3 | `docs/design/one-id.md` |
| A4 | 261113-083755-c5e4b4; CI green at 3d968d72c |
| A5 | 261113-083814-ba7801 |

## Result

A document's id is its only id (docs/design/one-id.md; how-it-works in docs/mechanisms/document-ids-and-versions.md,
user docs in index-api.md and architecture.md). Strata numbers new documents in append order; ids route by 1,024
buckets (`bucket % n`); each shard issues automatic ids from a persisted, replayed high-water counter
(`_id_high`), above every numeric id in the index. Numeric ids with buckets are 1..2^32-1 (R4; owner P2 pending).

- A1: after RESTORE 1..300 and PUTs of 11,000,001..11,000,030 and a merge, every shard holds one kind 0 segment
  (n=1: docids 1-330; n=3: docids 1-110 per shard). The named-docid mutant (new ids at the docid they name) fails it.
- A2, A6, A7: tests with exact values pass; each of the six mutants (no-persist, no-replay, no-advance x2,
  no-stride, hash-route) makes its test fail.
- A5 (ABBA, 2 pairs, master 396204136 with lanes = pre, one-id 8d61b1913 = cur, 64 heavy clients):
  - loaded light query p99: 61.2 ms pre vs 60.3 ms cur (cur/pre 0.93-1.06, median 0.99): no change. A miss: the
    harness bound per run is 7.7-19.7 ms, and every run is ~54-69 ms. Recorded as P1's update in PLAN.md
    (Xapiand:81b99d0ce): light queries wait off the loop (loop lag p99 ~4 ms) behind RESTORE's write share.
  - heavy searches: 23,085 pre vs 27,099 cur, 17% more (per pair 1.16 and 1.19).
  - restored docs: 80,000 in every run.
  - unloaded light query p99: 3.7 ms pre vs 7.2 ms cur (per pair 1.50 and 2.30). Not explained: two pairs, and
    the cur runs drift (5.0 then 9.3 ms); stated as a possible regression for the P1 diagnosis to rule on.

## Rulings I made

- R7 `open_blocking` reads `_id_high` with `engine::sync_wait`: it runs in the catalog's open chore on a WorkPool
  worker before the replay (never the loop), and strata's `metadata()` finds the key in the open view's map without
  suspending or I/O. Own allowlist row (harness/async_allowlist.tsv:25) rather than a raised count on the general one.

- R1 The id counter is one high-water mark per shard (`_id_high` metadata), not 1024 stored counters: every bucket's
  counter is derived from it, which is what keeps automatic ids above every numeric id present (contract) while D85's
  per-bucket stride holds. Equivalent under reshard (move a bucket: raise the target's mark).
- R2 Auto ids are also above every other shard's counter: `least_used_shard` already takes one turn per shard, and
  now reads each counter there and passes the largest as a floor (DB_SLOT_SHARDS third value).
- R3 An automatic id is written with expected version 0 (must not exist): a counter that fell behind conflicts,
  never overwrites.
- R4 Numeric ids with buckets are integers 1..2^32-1, the range the numeric key names exactly (2^32 + 1's key names
  2^32 + 2^24, job 261113-081918-7c337f); others hash. Was 1..2^32-2 (docid range). A counter at 2^32 - 1 issues
  nothing: a POST then fails (invalid), documented in index-api.md. Owner decision P2 (PLAN.md) pending; the lead keeps this bound as default (a).
- R5 `DatabaseHandler::replace_document(string_view)` (`&N` creating at a caller's docid) removed: no callers.
- R6 WAL replay raises the counter from the replayed document's stored `_id` (engine::Document has no key getter, and
  src/engine/document.h is outside the file set).
