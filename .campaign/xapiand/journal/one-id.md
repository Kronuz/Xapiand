# one-id: journal

State: waiting on jobs 261113-083755-c5e4b4 261113-083755-2de9cb 261113-083758-e58dc8 261113-083800-510abb 261113-083802-c6d044 261113-083805-101e1b 261113-083807-92969a 261113-083809-734db4 261113-083812-67e343 261113-083814-ba7801
Quest: .campaign/xapiand/quests/one-id.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs at 8d61b1913 (branch pushed for CI):

- 261113-083755-c5e4b4 build-test, tagged A1, A2, A4, A6, A7.
- Mutants (`tests/one_id_mutants.sh`, --fails-without, after the build-test): A1 named-docid 261113-083755-2de9cb;
  A2 no-persist 261113-083758-e58dc8, no-replay 261113-083800-510abb (test_pool), no-advance 261113-083802-c6d044,
  no-stride 261113-083805-101e1b (test_node); A7 hash-route 261113-083807-92969a; A6 no-advance 261113-083809-734db4.
- A5: release builds master 261113-080915-1d19d5 (done), head 261113-083812-67e343, then the exclusive ABBA
  261113-083814-ba7801 (.scratch/ab/ab.sh, arms pre/cur).

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

1. On green: paste `campaign brief`'s table, CI result for A4, A5 numbers into the Result (a miss stated as one).
2. Correctness review is the lead's to launch.

## Blocked on

Nothing blocking. For the lead (finding, pre-existing on master's encoding, outside this quest): explicit PUTs of
4294967297 and 4311744512 name one key, so one overwrites the other. The serialisation is GPL code I may not read;
license-closure's replacement could make the key exact to 2^53, and MAX_NUMERIC_ID would follow it.

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | `docs/design/one-id.md` |

## Result

<pending>

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
