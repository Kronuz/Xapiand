# one-id: journal

State: waiting on jobs 261113-082922-3f2ebf 261113-082930-72d60f 261113-082932-b0d7dd 261113-082935-ecda23 261113-082937-a0d792 261113-082940-a6258e 261113-082942-7818c1 261113-082945-4f215a 261113-082947-f7da32 261113-082953-82e3f3
Quest: .campaign/xapiand/quests/one-id.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs at 2890b08db (branch pushed for CI):

- 261113-082922-3f2ebf build-test, tagged A1, A2, A4, A6, A7.
- Mutants (`tests/one_id_mutants.sh`, --fails-without, after the build-test): A1 named-docid 261113-082930-72d60f;
  A2 no-persist 261113-082932-b0d7dd, no-replay 261113-082935-ecda23 (test_pool), no-advance 261113-082937-a0d792,
  no-stride 261113-082940-a6258e (test_node); A7 hash-route 261113-082942-7818c1; A6 no-advance 261113-082945-4f215a.
- A5: release builds master 261113-080915-1d19d5 (done), head 261113-082947-f7da32, then the exclusive ABBA
  261113-082953-82e3f3 (.scratch/ab/ab.sh, arms pre/cur).

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

- R1 The id counter is one high-water mark per shard (`_id_high` metadata), not 1024 stored counters: every bucket's
  counter is derived from it, which is what keeps automatic ids above every numeric id present (contract) while D85's
  per-bucket stride holds. Equivalent under reshard (move a bucket: raise the target's mark).
- R2 Auto ids are also above every other shard's counter: `least_used_shard` already takes one turn per shard, and
  now reads each counter there and passes the largest as a floor (DB_SLOT_SHARDS third value).
- R3 An automatic id is written with expected version 0 (must not exist): a counter that fell behind conflicts,
  never overwrites.
- R4 Numeric ids with buckets are integers 1..2^32-1, the range the numeric key names exactly (2^32 + 1's key names
  2^32 + 2^24, job 261113-081918-7c337f); others hash. Was 1..2^32-2 (docid range). A counter at 2^32 - 1 issues
  nothing: a POST then fails (invalid), documented in index-api.md. Owner may want a different policy (lead).
- R5 `DatabaseHandler::replace_document(string_view)` (`&N` creating at a caller's docid) removed: no callers.
- R6 WAL replay raises the counter from the replayed document's stored `_id` (engine::Document has no key getter, and
  src/engine/document.h is outside the file set).
