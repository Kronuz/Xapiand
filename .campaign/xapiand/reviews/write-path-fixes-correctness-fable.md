# write-path-fixes review: correctness (fable)

Commit: 60a5e373643b8253c7a91e608d7252eda46d180a
Verdict: CHANGES
Date: 2026-11-10

Second, independent correctness review (D49 paired-model trial); the other reviewer's work was not seen.
Lands after WPF-F1 is fixed and confirmed by a rerun of the probe below at the fixed commit.

## Evidence I relied on

- 261110-045430-4ed4e9 (exit 0, head 60a5e3736): `xapiand_test_engine_strata` (9768 checks, `conflicts` and
  `placement` included) and the whole `xapiand_test_node` (454 checks): the four new cases pass, the model case
  reports "48 writes won, 44 refused". Read from its log, not the journal.
- 261109-202740-e5400a and 261110-022045-f81f3a (the base with only the tests): the id case aborts at
  `assert(did != 0)` with asserts, 48 of 176 id checks and 33 of 111 precondition checks fail without; the mutant
  jobs a-i each exit 0 only when their named test fails. Enough for "fails without the fix" on A1 and A2.
- 261110-045411-83e91d (build-test: ctest 37 of 37, async_check, removed_options) and 261110-045423-57a7f5
  (docs E2E smoke, e2e, tls GREEN) at the head: A3 and the build half of A5.
- 261110-045424-bc00ed (TSan, llvm@22): the four node cases three times each and the strata test, 0 reports: A4.

## What I checked myself

Scripts in `.scratch/review-write-path-fixes-correctness-fable/` (`run.sh` builds `wal_placed_replay.cc` in place of
`tests/test_wal_replay.cc` in the job's checkout; the rerun command is in `run.sh`):

- 261110-052947-b25f7d (exit 1) and its rerun 261110-063240-7f25ee: on an engine shard, as the host drives it, the
  writes the quest adds and the WAL line the host writes after each applied one: a key placed at the docid it
  names (`WriteArgs::new_did`), one numbered after the last, refused writes and erases with an expected version
  (no line, nothing changed, `current_version` 1 or 0 as the case says), applied ones (version 2, then erased
  expecting 2), a key placed at the freed docid, a key re-created once its docid is taken (after the last), a
  missing key created expecting 0. The seven lines replay through the real `DatabaseWAL::execute_line` into a
  fresh shard twice: "all ok" and the replica's state equals the primary's after each pass
  (`5:v1:nine;6:v1:z;7:v1:five-3;8:v1:m;last=8,count=4`). The one FAIL in b25f7d (`lines.size() == 6`) is the
  probe's own miscount (seven lines), fixed in the rerun.
- The same job, `far_docid`: a key placed at 1,000,000 and at 100,000,000 costs 42 and 34 ms and no RSS (the
  index is sparse), and the next add numbers after it. Placed at 4294967295 (`docid::max`, the top of
  `numeric_docid`'s range): the next add answers `did 0`, the stats read `last=0 count=6`, `get(0)` is
  doc_not_found while `get(4294967295)` reads back. WPF-F1.

Read, with the risks the lead named:

1. Atomicity: the expected version is compared in the engine, inside the index's turn, for every path:
   `check_version` in `src/engine/strata/shard.cc:884` (replace by docid), `:937` (replace by key), `:976`
   (erase, when local), `:1006` (erase by key); the host's writes reach them only through `Shard::*`, which take
   the turn (`src/database/shard.cc:1452, 1470, 1509, 1529`). The early checks in `prepare`, `patch` and
   `update` raise the same error and the engine checks again. Writers of one key meet on one shard: an existing
   document is replaced where `get_docid_term` finds it; a new key is placed by a function of the key alone
   (`numeric_docid` or the key's hash), and a numbered id's key is derived from its shard so it hashes back to
   it. The `replace(did)` path verifies in the turn that the key still belongs to `did` (`:856-871`, the new
   `version_conflict(0)` when the docid holds another document); `erase(did)` cannot (WPF-F2).
2. Id 0: the three places that read the serialised 0 as "no id" are replaced (`has_document_id`,
   `NEW_NUMERIC_KEY`, `replace_by_key` for every explicit id); GET and DELETE resolve by key (`to_docid` only
   serves `&<docid>`); the test writes -1, 0, 1, 2, 2^63-1, 0 and 2^64-1 (positive), "a", "0", "00", "-1"
   (keyword), reads each back at version 1 with its own `n`, searches each by `_id`, and deletes them one by one
   while the rest stay. The quest's "largest integer id" (2^63-1) is placed by key and round trips; ids past
   2^53 share a key (WPF-F5, pre-existing).
3. WAL: the line is written after the engine applied (`src/database/shard.cc:845-857, 877-886`), never on the
   exception path, with the engine's docid; replay goes through `ReplayTarget` (`:415-427`) into `do_replace`
   with `version=false` and `do_delete` with no expected version, so a replayed line is never refused by a
   version; `replay_applies` (`strata/shard.cc:719`) skips what the index already holds. The probe above covers
   the new `replace_docid`-for-a-new-key op and the versioned erase.
4. Version: `info.version = stored + 1` once per applied write; a refused write throws before the transaction
   is built; `engine::version_conflict(current)` carries `current_version`, which `on_error` puts in the 409
   body as `_version` (`search_views.cc:2159, 3232-3239`); the precondition test checks `_version` 2 and 0 in
   409 bodies and the document unchanged at `v: 2`, the model test `win.version == seen + 1`.
5. Without `?version`: the retry condition is master's (`handler.cc:491, 596, 649, 728` unchanged); the
   unversioned paths differ only in (a) a numeric id whose docid holds another document no longer overwrites it,
   (b) multi-shard writes read `get_docid_term` first for numeric ids too (WPF-F6), (c) `upload_stored` takes id
   0. build-test and the docs E2E pass at the head.

Also: `DocVersionConflictError` has no remaining thrower (grep of `src`, `tests`); `catch_http_errors` answers
409 for `errc::version_conflict` (`http_utils.h:68`, `test_engine_error.cc:121`). The WAL replay adapter's
`delete_document(did, bool, bool, bool)` takes `do_delete`'s default `std::nullopt`, so the signature change does
not turn a replayed delete into an expected-version-0 erase (checked, since `bool` converts to
`std::optional<rev>` silently).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| WPF-F1 | yes | defect | The shard's numbering wraps past `docid::max`, and `numeric_docid` lets an explicit id take that docid. `numeric_docid` (`src/database/handler.cc:127-137`) accepts ids up to `docid::max`; `replace_by_key` places such a key at docid 4294967295; the next numbered write (`add`, `strata/shard.cc:820`, or a new key after the last, `:930`: `last_docid() + 1` cast to `docid`) lands at docid 0: unreadable (`get(0)` doc_not_found), `lastdocid` reads 0, the count grows. Over HTTP: `PUT /i/4294967295` then any `POST /i/` or keyword-id write in that shard. Pre-existing in shape (master placed numeric ids at docid = id), but the quest writes the range down and places by it; the limit is not load-bearing. | 261110-052947-b25f7d (`far docid 4294967295: ... add -> did 0; last=0`); rerun 261110-063240-7f25ee, exit 1: 42 of 45 checks pass (the whole WAL replay part), the three failures are this step's `did != 0`, `lastdocid != 0`, `lastdocid >= target` | The engine refuses to number past `docid::max` (an `engine::Error`, say `errc::database` "no document id left", at `:820` and `:930`) instead of wrapping; with a test. And `numeric_docid` stops short of the top (`id < docid::max`, or a margin) so id 0xFFFFFFFF is placed by key and does not exhaust the shard's numbering; the `new_did` placement stays as ruled. Confirm by rerunning the probe (`run.sh`): it passes once no write lands at docid 0. |
| WPF-F2 | no | defect | Multi-shard keyed DELETE resolves the docid outside the turn and erases by docid without the key: `handler.cc:1817-1819` (`get_docid_term` then `delete_document(did)`) reaches `eng.erase(shard_did, true, expected)` (`database/shard.cc:871`), which checks only the version of whatever is at that docid (`strata/shard.cc:965-985`). Between the read and the turn, a delete of the key plus a new numeric id placed at the freed docid (`new_did`) makes the DELETE, versioned too (the new document is at version 1), remove the other document and answer 204; the quest's `replace(did)` guards the same window with the key (`:856-871`), `erase(did)` has no key. Single-shard deletes go by key (`erase_by_key`) and are not exposed. Same shape at master. | file:line (no probe: needs a sharded index and the race) | Options: on the shard `get_docid_term` found, call `delete_document_term(term, …, expected)` (erase by key, as the single-shard path does) instead of `delete_document(did)`; or give `erase(did)` the key to verify, as `replace(did)` does. |
| WPF-F3 | no | nit | Version numbers restart at 1 when a document is deleted and re-created, so `?version=1` matches a re-created incarnation the client never saw (ABA); Elasticsearch's `if_seq_no` is monotonic per index. Pre-existing; a design note for later, not this quest. | `strata/shard.cc:938` (`stored.value_or(0) + 1`) | None here; note it in the mechanism doc if wanted. |
| WPF-F4 | no | nit | `docs/status/engine-search.md:199, 390` and `docs/status/engine-core.md:83` still describe `DocVersionConflictError`, deleted at this commit. | grep | Drop or reword the three mentions. |
| WPF-F5 | no | nit | Ids past 2^53 share a key (the numeric id is a sortable double): 9223372036854775806 and 9223372036854775807 are one document. The journal's ruling 6 names it; pre-existing; the sortable-encoding quest's area. | `Serialise::serialise` of numeric ids | Leave to sortable-encoding. |
| WPF-F6 | no | nit | Multi-shard keyed writes now read `get_docid_term` across every shard before placing, numeric ids included, where master computed the shard from the id (`handler.cc:1904-1935`); one read per write, correctness-neutral. `DB_SLOT_SHARDS` is now set for every key and cleared by `finalise_write`. | file:line | Keep, or route in-range numeric ids straight to `(k-1) % n` since an existing one is always there. |
| WPF-F7 | no | nit | `erase_by_key` checks the expected version regardless of `local`; `erase` only when `local` (`strata/shard.cc:975-977` vs `:1006`). Harmless (replay uses `erase(did, true, nullopt)`); one shape would be better. | file:line | Align the two. |

## Not run

- CI: the branch is not pushed; A5's CI half is the lead's.
- A sharded index over HTTP: the multi-shard placement and WPF-F2's race were read, not run.
- Any Xapian source or `src/sortable_serialise.*` (GPL): the numeric key's value semantics are taken from the
  tests and the probe's round trips.
- The RESTORE/bulk path with versions (it writes with `version=false`, outside the quest's `?version` contract).
