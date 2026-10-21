# engine-core (X2, phase 0.5)

Branch `phase0.5/engine-core` from Xapiand master `5cce4860d`. Design:
`strata/docs/design/xapiand-on-strata.md` sections 3 and 4 (row X2), D13, gate rows T2 to T4.

## State

X2 is split at the design's seam. Half A (this launch) is complete and verified: the seam's types,
errors, `Document` and `DocView`, the Xapian implementation, and every `Document` producer and
consumer on the write path (schema, generate_terms document side, handler prepare/index, Shard
document writes, WAL). Half B (plan below) is not started and is blocked on `task` (see Blocked on).

## Done criteria (design row X2)

`src/engine/` with types, errors, `Document`, `DocView`, the `Shard` write and lifecycle slice, Xapian
implementation; `schema.cc`, `generate_terms.cc` (document side), `handler.cc` prepare and index,
`shard.{h,cc}` writes, `wal.{h,cc}`, `pool`, `flags.h`, `exception_xapian.h` migrated; `Shard::db()`
still exists for reads; E2E and WAL recovery unchanged.

## Done (half A)

- `src/engine/{types,error,document,doc_view}.h`: ids and counts, open actions, `errc`/`Error`, the
  neutral `Document` (same semantics as the Xapian calls it replaces), `DocView`, `WriteResult`.
- `src/engine/xapian/convert.{h,cc}`: `to_xapian`/`from_xapian` for documents and write results,
  `classify_current`/`translate_current` (Xapian exceptions to `errc`; `Xapian::Error` is not
  polymorphic, so classification is a rethrow-and-catch ladder), `make_view`, and `Document::serialise`
  / `unserialise` through Xapian's own format (old WAL lines stay readable). Static asserts pin the
  seam's types and open actions to Xapian's.
- Migrated: `schema.{h,cc}` (no `Xapian::` left in schema.cc), `generate_terms.{h,cc}` document side
  (the `USE_ICU` word-break path generates into a Xapian document and merges), `handler.{h,cc}`
  prepare/index/replace and the bulk-index queues, `shard.{h,cc}` writes (conversion happens at the
  database call; the Xapian-typed overloads stay because the Xapian remote server calls them, confirmed
  by `nm` on its object), `wal.{h,cc}`, `utils.{h,cc}`, `flags.h`.
- Tests: `tests/test_engine_document.cc` (840 checks): 200 random call sequences against a Xapian
  document compare serialised bytes and the read-back; classification table; `DocView`; word-break merge.
- Commits: `7eae6786c` (seam, types, errors, Document, DocView, write-path migration).

## Proof (half A)

- E2E: 339 requests, 576/576 assertions, `e2e_assert.py` GREEN; `e2e_diff.py` against the X1 final
  capture GREEN (0 regressions, 0 status differences); `bodydiff.py` differs in 4 volatile bodies only
  (generated id, node port, build revision, metrics).
- Text analysis differential (`terms_diff.py`, `vocab_diff.py`): output byte-identical to the X1 final.
- `wal_recovery_check.sh`: PASS (50/50 recovered). `cluster_check.sh 2` and `3`: GREEN.
- ASan+UBSan build (`-fsanitize=address,undefined`): unit test 840/840, `wal_recovery_check` PASS, E2E 576/576
  GREEN and `e2e_diff` GREEN. The node log has 6 UBSan lines, all in third-party code (`lz4.c` zero offset
  to null, `rapidjson` stack.h null offset); none in `src/engine`, schema, shard, WAL or handler.
- Mutants, each caught: empty value kept (231 of 840 checks fail), first-position wdf (326), word-break
  merge drops positions (1), classify order (2), version value dropped in `to_xapian` (E2E: 17
  assertions regress, NOT GREEN).
- Seam count (`git grep` for `Xapian::`, `xapian/` or `"xapian.h"` in `src` outside `src/xapian` and
  `src/engine/xapian`): files 56 -> 52; `Xapian::` lines 1557 -> 1418. Out of the list: `schema.cc`,
  `utils.h`, `utils.cc`, `flags.h`.

## Next (half B: engine::Shard lifecycle and write slice)

1. `engine::Shard` (src/engine/shard.h): lifecycle (open, reopen, close, commit, transactions),
   `add`/`replace`/`replace_by_key`/`erase`/`erase_by_key`, metadata, `Stats`; `engine::xapian::Shard`
   owns the `Xapian::Database`; `Shard::db()` stays as `native<>` for reads (X4 removes it).
2. Extract the version dance (three copies in `shard.cc`, one more in the remote server's call path) into
   the Xapian implementation: `Document::set_key`, `expected_version`, `WriteResult`.
3. Errors in catches: replace the roughly 250 `catch (const Xapian::...)` lines (shard.cc, handler.cc,
   pool.cc, schemas_lru.cc, index_resolver_lru.cc, manager.cc, search_views.cc, http_utils.h) with
   `engine::Error` kinds, and rewrite `exception_xapian.h` over the kinds. Hazard: `engine::Error` derives
   `std::runtime_error`, and the wrapper classes derive `ClientError` (an `std::exception`) too, so a
   wrapper that also derives `engine::Error` has two `std::exception` bases and `catch (std::exception&)`
   becomes ambiguous; give the wrappers an `errc` member instead.
4. `pool` (`pool.{h,cc}`, `Xapian::rev` and catches), `DB_RETRIES` loops stay in Shard.
5. Per D13, every blocking seam call returns `task::task<T>`: Xapian implementation does
   `co_await task::schedule_on{offload}` then the blocking call; callers bridge with `sync_wait` on
   the offload thread.
6. Extraction-ledger row for the three version dances collapsing (the first row owed by the design).

## Blocked on

Half B item 5 (D13): `Kronuz/task` (`~/code/Kronuz/task`, `bbb2e14`) has no `sync_wait`. Its only
blocking driver is `task::run(inline_scheduler&, task<T>)`, which drains a same-thread queue and then
dereferences the result; if the task's continuation was posted to another thread (the offload pool),
`run` returns early with an empty result. Needed: a `sync_wait(task<T>)` that blocks the calling thread
until completion from any thread (spawn plus a mutex and condition variable would do, about ten lines).
A completion source is not required for the Xapian implementation: a coroutine that
`co_await schedule_on{pool}` then calls Xapian completes the task itself. Also needed host-side (not
in `task`): a `task::scheduler` over Xapiand's offload pool (`post(handle)`, `now()`); `task_asio.hh`
only covers Asio executors. Types, errors and `Document` are unaffected; half B waits for the lead's
decision on adding `sync_wait` to `task`.
