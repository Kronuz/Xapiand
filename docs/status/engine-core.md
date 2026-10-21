# engine-core (X2, phase 0.5)

Branch `phase0.5/engine-core` from Xapiand master `5cce4860d`. Design:
`strata/docs/design/xapiand-on-strata.md` sections 3 and 4 (row X2), D13, gate rows T2 to T4.

## State

X2 is complete in two halves. Half A (7eae6786c..d77cf2e0b): types, errors, `Document`, `DocView`, the
Xapian implementation and the `Document` write path. Half B (3be508883): `engine::Shard` (write and
lifecycle slice, `task::task<T>` per D13), its Xapian implementation with the version dance collapsed to
one, opening, error kinds in the retry ladders, pool, `exception_xapian.h`. `Shard::db()` still serves
reads (X4). Proof for half B is below.

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

## Done (half B)

- `src/engine/shard.h`: `engine::Shard` returning `task::task<T>`: `add`, `replace`, `replace_by_key`,
  `erase`, `erase_by_key`, `commit` (with transaction mode), transactions, `set_metadata`, spelling,
  `reopen`; cheap `revision`/`uuid`/`refs` plain. `WriteArgs` carries `local`, `expected_version` and a
  `finalise` hook (called after the target and the new version are known, before the write).
- `src/engine/xapian/shard.{h,cc}`: the Xapian implementation. One `stored_version`/`check_version`/`stamp`
  replaces the three version dances in `Shard::{add,replace,replace_by_key}` (and delete's unused check).
  Each operation does `co_await Hop{}` (onto the `Offload` pool when set, else nothing) then the blocking
  call, translating Xapian exceptions with `rethrow_translated`. `open_local`, `open_remote` and
  `open_local_if_same` do the Xapian opening.
- `Offload`: a small thread pool modelling `task::scheduler`. Off by default (calls run on the awaiting
  thread); `XAPIAND_ENGINE_OFFLOAD_THREADS=N` turns it on. Reason for the default: a caller that is itself
  an offload worker and waits with `sync_wait` on work posted to the same pool deadlocks when the pool is
  full, and a separate bounded pool would cap concurrency; both are the lead's call, so the path exists,
  is tested, and is off.
- `src/engine/xapian/errors.h`: `Failure<kind, XapianType>` derives from `engine::Error` and the Xapian
  exception it stands for (copying the original, so the system error string survives: `catch_http_errors`
  reads it for gateway errors). `engine::Error` is now a mixin, not a `std::exception` (the wrapper classes
  in `exception_xapian.h` also derive `ClientError`, an `std::exception`). New kind `database` for the
  generic Xapian database failure that closes the shard.
- `Shard` (database/shard.cc): `database` is now `engine_shard`; all write, lifecycle and open paths call
  the engine through `task::sync_wait` on the calling thread; the nine retry ladders are `retry_or_throw`
  on `engine::Error` kinds; `reopen` and `reopen_*` use kinds; `finalise_write` embeds the version and a
  new numeric id in the stored data. The Xapian-typed overloads for the remote server stay.
- `exception_xapian.h`: `DocVersionConflictError`, `NetworkError`, `InvalidArgumentError`,
  `InvalidOperationError`, `QueryParserError` also derive `engine::Error` with their kind. `pool.cc` throws
  `NotAvailable`/`Opening`. `handler.cc` (prepare's conflict, three retry loops) and `index_resolver_lru.cc`
  retry on the `version_conflict` kind.
- Pinned `Kronuz/task` at `aa447e726c617c5c10a569409ecf8e024b88750f` (CMake FetchContent, header-only).
- Commit: `3be508883`.

## Proof (half B)

- E2E 339 requests, 576/576 assertions GREEN; `e2e_diff` GREEN against the X1 final capture; also with
  `XAPIAND_ENGINE_OFFLOAD_THREADS=4` (same result).
- `terms_diff`/`vocab_diff` byte-identical to X1 final. `wal_recovery_check.sh` PASS (inline and offload).
  `cluster_check.sh 2` and `3` GREEN (offload on).
- `tests/test_engine_shard.cc` (52 checks) runs versioned writes, conflicts by kind (also catchable as the
  Xapian exception), by-key writes, erase, commit/transactions/metadata/reopen, not-found open, inline and
  with the pool (and checks the call really ran on another thread).
- ASan+UBSan build: `test_engine_document` 844 and `test_engine_shard` 52 checks pass, `wal_recovery_check` PASS,
  E2E 576/576 GREEN and `e2e_diff` GREEN, also with `XAPIAND_ENGINE_OFFLOAD_THREADS=4`; no sanitizer report
  outside the third-party lz4 and rapidjson lines seen in half A.
- Mutants, each caught: version check always passes (8 of 52 checks fail), version not bumped (aborts on
  the first conflict), offload hop removed (1), erase-by-key lookup skipped (2), embedding of the version
  in stored data dropped in `finalise_write` (E2E: 16 assertions regress, NOT GREEN).
- Seam metric (files / lines naming `Xapian::`, `#include "xapian.h"` or `#include "xapian/` outside
  `src/xapian` and `src/engine/xapian`; includes of `engine/xapian/` not counted): master 56 / 1614, after
  half A 52 / 1474, after half B 52 / 1297. `shard.cc`: 215 `Xapian::` references -> 55; `pool.cc` 17 -> 4.
  `shard.cc` 2163 -> 1940 lines. `catch (const Xapian::...)` lines (without the remote server): 245 -> 208.
- Behaviour note: a retry after a failed write compared the version it had just assigned against the
  stored one and raised a conflict; the engine keeps the caller's expected version across retries, so the
  retry no longer fails that way. Not reachable from the E2E suite (needs a database error mid-write).

## Next

1. X4 removes `Shard::db()`: the reads (`get_document`, `get_docid_term`, metadata reads, `check`), their
   Xapian catch ladders (about 110 lines across handler.cc, shard.cc, search_views.cc, schemas_lru.cc,
   manager.cc, lua, http_utils.h) and `catch_http_errors` move to kinds. Until then they keep catching the
   Xapian types, which every engine failure still is.
2. Callers that become coroutines (7c) drop their `sync_wait` one site at a time.
3. Decide the offload default (see above) from the lead's measurements.
4. The ten raw `throw Xapian::DocNotFoundError` sites (handler.cc, shard.cc, search_views.cc) move to a
   `DocNotFound` wrapper with the kind, then their catches move to `doc_not_found`.

## Blocked on

Nothing.
