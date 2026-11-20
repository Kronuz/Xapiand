# cheap-reopen review: correctness

Commit: Xapiand:e7584201c407
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- A2: 261119-112059-d1220b (pool 200 at 2.2x of pool 5000, D135). A3/A4: suite 261119-112102-8426f3 at
  a152bf614 (only merges from main since), including tests/test_cheap_reopen.cc (evicted, crashed, missing,
  8 partitions).
- CI run 44447325291 at 937a5e94e (contains D1, D2 and D3): green, including "Unit tests under TSan" and
  ASan + UBSan. CI at the head e7584201c was still running when I wrote this.

## What I checked myself

By reading the diff (src/database/pool.cc, pool.h, handler.cc) against the strata catalog pinned at 397c02d5:

1. Stale reads after reopen: this quest caches nothing across a close. D4's remembered log state is not
   built, the catalog's ghost holds hashes only, and a reopen runs strata `recover`, the WAL replay and the
   DataStorage scan from disk as before. A key maps to one catalog through a deterministic hash of the key
   (pool.cc:657-666), so an index is never open in two catalogs, and the directory lock still excludes other
   processes. Negative entries are per catalog, and a key always goes to the same catalog, so they behave as
   before.
2. Eviction under an active pin: D1 doesn't change any slot's lifetime rule. The sweep takes only `{0 pins,
   open}` slots. D3 holds every pin of a batch until the batch ends, de-duplicates repeated paths
   (handler.cc:1475-1482) and keeps `shards` reserved, so the references the pins take stay valid. `when_all`
   rethrows after every pin ends, and `~lock_shard` releases the others.
3. Shared state across lanes, which now run concurrently: strata's `resource_budget` keeps a listener list
   under a mutex and splits excess per catalog (catalog.hh:2469, built for several catalogs). The Timer locks
   `pending_mtx`. `NameIndex::opened` takes its `shared_mutex`. `inline_chores()` is stateless. The WAL writer
   is `thread_local`. `ShardEndpoint::open_blocking` works on one index. Concurrent `create_directories` for
   nested keys (twitter, twitter/user) treats EEXIST on a directory as success.
4. Shutdown of P catalogs: `pending` is set before any partition's coroutine can finish inline,
   `done = --pending == 0` is set under the mutex, and `failed` is concatenated (pool.cc:696-711).
5. D2: ENOENT/ENOTDIR become `index_not_created`, and any other error is rethrown (pool.cc:571-583). For nested
   paths, a parent that has only a child index has no `strata` directory, so it is not_found, as before.

Probe job 261119-145747-3e9285 (scripts in `.scratch/xapiand/review-cheap-reopen-correctness/`: `run_probe.sh`
builds `test_zz_probe_reopen_route.cc` at the quest commit and runs it):

- Without `--uuid-partition`, `Endpoint(key).path == key` for all 8 keys, so `IndexPin::reopen`, which
  routes by `payload().path`, reaches the catalog that pinned the key.
- With `--uuid-partition`, the same holds for 0 of 8 keys: each UUID segment is partitioned again
  (`probe_route/4526/10c3/<uuid>` becomes `probe_route/4526/10c3/4526/10c3/<uuid>`). Every pin of such an index
  then fails with `StorageNotFound` from DataStorage on the ShardEndpoint's doubly partitioned path, and the
  child process aborts on it. That failure comes before any pin exists, so F1's misrouting can't be reached
  today.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | `IndexPin::reopen` picks its catalog by `pin_.payload().path` (pool.cc:862), but the pin was taken with `catalog(endpoint.path)`. They are the same partition only while `Endpoint(key).path == key`. That is false under `--uuid-partition`, so with P > 1 the reopen would run on another partition's catalog (wrong shard mutex, close posted to the wrong group). It can't be reached today, because F2 fails the open first; it becomes live as soon as F2 is fixed. | probe 261119-145747-3e9285; pool.cc:859-865 | Have `IndexPin` keep the `IndexCatalog&` (or the key) it was pinned through, and route `reopen()` by that. Test: in test_cheap_reopen, a pool of 8 partitions, `reopen()` of a pin whose `payload().path` differs from its key (or simply: the key's catalog counts the replace). |
| F2 | no | defect | Pre-existing, outside this quest: under `--uuid-partition` a UUID-named index can't be opened at all. The ShardEndpoint re-parses its key and partitions it twice, so DataStorage and the WAL look in a path that doesn't exist. `flush()` (`pin_chore(Endpoint(slot.key))`, pool.cc:939) would also miss those keys. Neither endpoint.cc nor shard.cc is in this diff. | probe 261119-145747-3e9285 (8 of 8 pins throw StorageNotFound) | A hunt row: make `Endpoint` idempotent on an already-normalized key, or build ShardEndpoint and flush from the key without re-parsing it. The owner decides whether `--uuid-partition` is kept. |
| F3 | no | gap | Concurrency across partitions has no test that forces it. Every test_pool case runs one partition (`one_catalog()`), and test_cheap_reopen's 8-partition case is sequential. Only test_node's searches (node pool 200, partitions limited by the runner's threads) reach several lanes under CI's TSan. By reading, every object shared across lanes is safe (item 3 above). | tests/test_pool.cc `one_catalog`; tests/test_cheap_reopen.cc:183 | Add a stress case to test_cheap_reopen: a pool of at least 2 partitions with a small soft_open, 4 threads writing, committing, reading and evicting keys spread over the partitions (nested a and a/b among them), values checked against a sequential model. CI's TSan job runs it. |
| N1 | no | nit | D1 splits soft_open evenly per partition, so the default pool (200 on 8 or more threads) evicts at 25 per partition even while other partitions are under their share. That is the design's choice, but batch pins can overshoot one partition (soft cap, never a deadlock: catalog.hh:1452). | pool.cc:643-649 | None needed. Mention it in the design note's D1. |

## Not run

- No local TSan or ASan run: I relied on CI at 937a5e94e. CI at the head e7584201c was still running.
- I didn't re-measure A2, re-run the suite or the crash test, or check strata's catalog internals beyond
  the reopen, sweep, budget and pin paths named above.
- Another process writing an index while this one has it evicted: settled by reading (full recover on every
  open, and the directory lock while open), not by a probe.
- One of my commands was refused by the permission classifier (a grep over src/ without
  `--exclude-dir=xapian`). I reran it with the exclusion.
