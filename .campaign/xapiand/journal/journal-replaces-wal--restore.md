# journal-replaces-wal--restore: journal

State: waiting on jobs 261122-230005-c6e3fb (tests), 261122-230005-d4c585, 261122-230006-5fc529, 261122-230006-38ba97 (mutants), all at 8649f1029
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

Session 1 (2026-11-21), base Xapiand:cf65ae78b (core steps 1-3 and b). Built, each its own commit:

1. `src/database/restore_batch.{h,cc}`: RestoreBatcher packs a shard's prepared writes in input order
   into commands within COMMAND_ENTRY_BYTES (one over it alone, for split_command), closes on a schema
   change, drops each write's expectation; `restore_results` reads docid/version per op from the
   verdict; `RestorePropose` is the marked seam for core step d. Test tests/test_restore_batch.cc.
2. `command.{h,cc}`: `encoded_size(const CommandWrite&)` only (Xapiand:3ceda7d87).
3. `shard_group.cc` `transaction()` only (Xapiand:371726159): a replace by key is stamped with the
   version it lands at (expected+1, else the key's version after the command's earlier writes or in
   the index, +1) when that differs from the document's: stamp, version slot, data `_version`.
4. tests/test_journal_replay.cc (was test_wal_replay.cc): erase of a missing doc is a no-op; replica
   from empty equals the primary; recovery from the capture after every entry (file_set::recover,
   state from bundle bytes, effects rerun, strata skipped) rebuilds files, state and locators, twice.
5. harness/journal_replay_check.sh (was wal_replay_check.sh, agg check folded in), outcome
   assertions only; wal_recovery_check.sh and wal_replay_agg_check.sh deleted.
6. harness/profile_bulk.sh: `--put`, `--clients`, `--query`, wall ms/doc, a window covering the smoke,
   a journal layer row.

## Session 2 (restore fix)

test_restore_batch failed 2 of 320 (:163, :188) at 2141ded67, fcd06fe60 and cc9f4b258 alike. Cause: the
test's bodies were `std::string(n, 'x')`; Data zstd-compresses what it stores, so a 1.5 MiB body shrank to
bytes and 300 writes fit one 64 KiB entry. Not the batcher, not leb128::size (same count as the old
encode-based number_size; the 4096 limit case passed with 7 commands). Fix, Xapiand:e3e55af98: `noise()`
incompressible bodies; the encoded_size check also asserts the body stays > 70000 bytes and compares each
write alone with encode_entry's size (the aggregate check already existed). Jobs at e3e55af98: tests
261121-171240-f68a69, the three mutants rerun there (the cc9f4b258 ones superseded).

## Session 3 (restore fix, round 2)

261121-171240-f68a69 at e3e55af98 failed 1 of 670: :159 `b.document.bytes.size() > 70000u`. noise() drew 90 printable
characters (~6.5 bits/byte), so zstd still shrank the 70,000-byte body. Probe 261121-203148-ccf367 prints the
stored sizes at e3e55af98 (pending). Fix, Xapiand:72efab739: noise() takes all 256 byte values (a MsgPack str carries
any bytes; Data stores a record raw when zstd does not shrink it); each case prints its sizes and checks its limit:

| Case | Limit it crosses | Check | Stored size |
|---|---|---|---|
| encoded_size, 70000-byte body | a 3-byte leb128 length (>= 2^14) and a body > 65536 | document > 70000 | pending 261121-203231-e1086b |
| packing at 4096 | entry limit 4096, several writes per entry | 1 < commands < writes, next write overflows | pending |
| packing at 65536 | entry limit COMMAND_ENTRY_BYTES/16 | same | pending |
| 1.5 MiB body | COMMAND_ENTRY_BYTES (1 MiB): a command alone, split_command > 2 chunks | encoded_size > 1 MiB | pending |

Mutants rerun at 72efab739 (the e3e55af98 ones superseded): 261121-203231-13b0f8, 261121-203232-74304e,
261121-203232-09b718.

msg-1: deferred to the next allowlist edit (this task does not edit harness/async_allowlist.tsv).

## Session 4 (jobs at 72efab739 read)

- 261121-203231-e1086b: pass, 4 of 4 (restore_batch, journal_replay, shard_group, group_command).
- 261121-203231-13b0f8 (expectation kept): killed, test_restore_batch.cc:190 `CHECK_FALSE(w.expected.has_value())`, 600 of 673.
- 261121-203232-09b718 (bundle drops the volume length): killed, test_journal_replay.cc:263-264, then file_set_damage abort.
- 261121-203232-74304e (no restamp at apply): SURVIVED. Log shows shard_group.cc.o rebuilt with the mutant and
  test_restore_batch passed; the :190 kill the lead read is 13b0f8's. The pipelined test does not see a stale `_version`.
- Probe 261121-203148-ccf367 (e3e55af98, 90-character noise): stored 57663 of 70000; the 1.5 MiB write encoded 1292087.
- Stored sizes at 72efab739 (13b0f8's log, same bodies): 70000-byte body -> document 70057, write 70302; packing
  writes 156-466 bytes, 93718 in all: 24 commands at 4096, 2 at 65536; 1.5 MiB body -> write 1572935 of 1048576.
- Quest jobs at cf65ae78b: 261121-105359-853976 pass 3 of 3; mutants 105428-7308c6 (group_command:169/171),
  105431-21e0e8 (shard_group:235/236), 105434-07ffd7 (group_prepare:165-179) killed; CI run 44938092439 success.

## Session 5 (restamp mutant: cause and fix)

- Probe 261122-211622-716e2b (restamp disabled, d2d7e8702): only two transaction() calls, both `Ka landing 1 native 1`,
  no `batch 2 verdict` line, no stored-version line, `OK: 673 checks`. Cause: test_harness.h's REQUIRE evaluates its
  condition twice, so `REQUIRE(r.apply(...).result == accepted)` at :246 applied the first entry twice; the second
  conflicted on Ka's expectation (0, Ka at 1) and REQUIRE returned with no FAIL line. Nothing after :246 ever ran:
  the pipelined test, its version and docid checks and the stored `_version` check were dead.
- Fix, Xapiand:ef2320627: the entry applies once, outside REQUIRE; the final loop prints each key's index version,
  stored `_version` and the wanted value, then checks both by value (Ka 3, Kb 2, Kc 2). Under the mutant three writes
  of batch 2 keep their prepared stamps (Ka 2, Kb 1, Kc 1 then 1), so the stored values differ.
- Merged origin/master, Xapiand:8649f1029: one conflict, CMakeLists.txt's strata pin; kept efa5214 (strata::group),
  which contains master's e3edb426.
- Jobs at 8649f1029: 261122-230005-c6e3fb (4 tests), mutants 261122-230005-d4c585 (expectation kept),
  261122-230006-5fc529 (bundle drops the volume length), 261122-230006-38ba97 (no restamp).

## Next

1. Read the five jobs (fill the stored sizes above from their logs); fix and rerun what fails.
2. At core step d: DocIndexer::apply_shards (handler.cc) feeds RestoreBatcher and proposes through
   RestorePropose, each command once the previous is placed; Shard::apply_slice and SliceWrite go.
   journal_replay_check.sh and profile_bulk's journal row get their evidence on that build
   (SETTLE=0; T11/T12 are the lead's measurements).

## Blocked on

- tests/test_harness.h (outside every task's file set): REQUIRE and REQUIRE_EQ evaluate their arguments twice, so a
  REQUIRE with a side effect runs it twice and can return silently (the failing evaluation's CHECK passed). Options:
  (a) evaluate once into a local (`const bool ok_ = (cond); CHECK(ok_); if (!ok_) return;`, the same for
  REQUIRE_EQ); (b) leave it and keep side effects out of REQUIRE. Recommend (a). The only side-effecting use found
  in tests/ was this test's :246 (now outside REQUIRE); `REQUIRE(node.ready())` in test_endpoint_uuid.cc and
  test_lua.cc look idempotent.

msg-204 and msg-205 answered by the lead (option (a) on both).

## Messages

- msg-1 (async_allowlist single_flight note, O(1) vs the leave's linear erase): deferred to the next allowlist
  edit; this task does not edit harness/async_allowlist.tsv.

- msg-204 (file set, renames, manager.*): answered by the lead, option (a). Done: restore_batch.*,
  test_restore_batch.cc, encoded_size in command.*, the renames, wal_recovery_check.sh and
  wal_replay_agg_check.sh deleted; manager.* untouched, ceded to core for step c.
- msg-205 (pipelined prepare): answered by the lead, option (a). Done: the batcher strips the
  expectation, results come from verdict.docids[i]/versions[i], transaction() stamps the landing
  version; the pipelining test with its mutant is 261121-111705-d17ab8 / 261121-111707-386655.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-203231-e1086b (test_journal_replay, plus test_shard_group and test_group_command after my two changes to core's files); 261121-203232-09b718 (mutant: the bundle drops the volume length, test_journal_replay fails); journal_replay_check.sh at the journal build pending step d |
| A5 | 261121-203231-e1086b (test_restore_batch); 261121-203231-13b0f8 (mutant: RESTORE writes keep the expectation, the pipelined batch 2 is refused); 261121-203232-74304e (mutant: no restamp at apply, stored _version stale); RESTORE through the group's propose pending step d |
| probe | 261123-014631-6ca9fe: journal_replay_check.sh and profile_bulk --put on the pre-journal build, mechanics only, not evidence |

Cancelled, superseded by the renames: 261121-110941-17b096, 261121-110943-f67445, 261121-110959-9c6a80.

## Result

<When ready for review.>

## Rulings I made

1. A refused batch fails every write of it with the refusal (the seam raises it): under (a) a
   RESTORE batch carries no expectation and no expected schema, so only `invalid`, `no_space` and
   the like refuse it, none of which a resubmit fixes; isolating the op and resubmitting the rest
   would reorder it behind later in-flight batches of the same keys.
2. The landing version is computed only for a write without an expectation; with one it is
   `expected + 1`, which prepare already stamped, so a PUT pays no key lookup and no restamp.
   Only replace_key (and erase_key, for a later write of the key in the same command) is tracked:
   RESTORE writes nothing else.
3. Batches are ordered by placement, not by apply: the seam's `placed` lets step d propose command
   k+1 once k is placed, so log order is input order with batches in flight. No window cap is
   set here; step d sets it with the propose path's admission.
4. journal_replay_check.sh and wal_recovery_check's successor kill -9 at once (SETTLE=0): an
   acknowledged write is durable at its answer, so a settle delay would hide a loss. The probe runs
   SETTLE=1 on the pre-journal build only to check the scripts' mechanics.
5. `Replica` (the test's index + file set + application) is copied into each test file rather than
   shared: a shared test header is outside every task's file set. Core may hoist it at step e.

## Open, for the lead

- `ack=none` answers at placement with no verdict, so a RESTORE at `none` has no docid or version
  for `_version`/`_xdocid` in its results (levels task, D4).
- docs/src/content/docs/tests/coverage.md:39 and harness/run_all.sh:191 still name the deleted WAL
  scripts (levels' files: run_all.sh gets power_loss_check.sh).
