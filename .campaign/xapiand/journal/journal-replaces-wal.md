# journal-replaces-wal: journal

State: waiting on jobs at Xapiand:a7b187c56 (not yet pushed: pushed once the drain 30x shows the build): drain probe 261124-033015-701cc3 (b2be5cac8), drain 30x 261124-033019-2a8cd9, drain mutant 261124-033023-f97bf7 (mutant/jrw-drain-destroy), replay check 261124-033415-7ce077, replay mutant 261124-033419-5c1d14 (mutant/jrw-recovery-report); still queued from session 13: R2 mutant 261124-021321-9231e2. Next: read them, push, rerun the evidence set at the head, `campaign verify`, then State "ready for review".
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

Session 14 (2026-11-24), the drain case's missing warning and the lead's replay-check message:

1. The drain case (015dd1385's test) missed only its warning (local 742fba run 1, CI TSan and ASan+UBSan; exit 75 in
   its bound). Cause by reading: the child slept 800 ms and assumed the first write's autocommit (200 ms after it)
   was suspended in a checkpoint behind the chore it then held; under load or a sanitizer the endpoints lookup and
   pin outlast 200 ms, the autocommit checkpoints first, so no control task is left at the deadline and Stop 3's
   close abandons instead (same exit, no warning). The code's path is right; the test's precondition was a race.
   Probe 701cc3 (Xapiand:b2be5cac8) logs the detached count as the shutdown starts and prints the child's log on a
   miss: a failing run must show 0. Fix Xapiand:cae4f2900: the child waits for the first autocommit to end, holds
   the chore (entered), writes at ack=none (202: placed, apply held, index modified), and starts the shutdown once
   detached() != 0, so the autocommit is in the held checkpoint whatever the timing. 30x 2a8cd9; mutant f97bf7
   (Xapiand:4dcc14fbd, 015dd1385's check off) must fail it.
2. Batch-pins verdict (probe 23729b): the case took 21.4 s at load 14, of which 886 F_FULLFSYNC calls took 22.2 s
   (per index ~37: the turns log 252, the journal directory 217, artifacts 192, the index's own 72+24+27+26). The
   hang under the crash gate was these flushes past the 40 s default watchdog, not a pin. Xapiand:bb8036409 gives the
   case 120 s (the wildcard case's bound). For the lead (strata/journal): fewer full flushes per create, write and
   checkpoint is the journal host's batching.
3. Lead msg (replay check 01a519, "a recovery that cannot write does not serve"): the node refused to serve, but died
   with "Uncaught exception: short turn log preallocation": journal's TurnLog::preallocate throws a bare
   runtime_error on the short write a file-size limit gives (no errno, no file). Verdict: the uncaught exception was
   the defect. Fix Xapiand:a7b187c56: the manager reports "The journal in .xapiand/journal cannot be recovered:
   <error> (the process's file-size limit is N bytes)" and ends the start with EX_IOERR; the check requires that
   report and no "Uncaught exception". Check 7ce077; mutant 5c1d14 (Xapiand:e68110ef7, the report removed) must
   fail the failure case. For the lead (journal library): a short write should continue to the error and name
   the file and errno (EFBIG); a hunt row, not this quest.

Session 13 (2026-11-24), the lead's relay (~/.scratch/xapiand/lead/jrw-relay.md) and R2:

1. CI at 34553fa85 (run 45308297820): Functional E2E (docs), ASan+UBSan and TSan all failed on the same 4 E2E
   assertions, this branch's own: docs_to_postman.py adds a bare `?commit` to every request that names none, and
   `?commit` beside `ack=none` answers 400 (D4), so basics.md's two ack=none examples failed and the linearizable get
   and prefix search after them saw nothing. The suites under both sanitizers were GREEN (failed: 0). Fix
   Xapiand:6d237b1af: the two e2e blocks name `params: commit=false` (quickstart's `echo=false` precedent); the
   generated requests checked locally (docs_to_postman.py on basics.md: `ack=none&commit=false`). Observation for
   the lead: the generator's blanket `?commit` is a WAL-era default; with D4 every docs write is refresh=true.
2. Crash check 261123-165000-bd1f5e (and fef4e8 at 6d6921068): test_node failed, the batch-pins case hung past its
   40 s watchdog (item 7 of session 12); every other test passed. Probe 9e4f83 attributes its flushes per file.
3. R2: strata pinned at 7a4284c4 (Xapiand:a2f35ed91; journal 9c7e31c, consensus 6e1122c come with it; no Xapiand
   use of the changed group internals). Test (Xapiand:ea5ac38e8) test_shard_group
   `test_a_seal_reads_its_volume_in_bounded_pieces`: an upload of 16 x 64 KiB + 7 applied as chunks and a final over
   an IO recording the largest read; the seal's CRC32C is the bytes' and no read exceeds 64 KiB. Mutant: strata at
   00eb018 through --ref (FETCHCONTENT_SOURCE_DIR_STRATA), the old whole-file seal.
4. The shutdown-drain abort (26f9f6, SIGABRT after "Exiting with unfinished writes abandoned", nothing printed).
   Cause by reading (strata group.hh at 7a4284c4): the drains end at the deadline with the two autocommits of
   `late` left (one suspended in the group checkpoint whose chore the test holds, one waiting behind it); the
   shutdown then closed the control scheduler and destroy_roots() freed their frames. strata's group keeps raw
   pointers into such frames (a chore `call_work` the awaiter owns, `waiters_`/`readers_` resumed by fail_waiters
   in tear_down), so Stop 3's close of `late` can resume or post a freed frame: a race with _Exit, hence once.
   Fix Xapiand:015dd1385: a control task left at the deadline abandons the stop with the loop left running (as an
   abandoned HTTP stop does), warning "The control loop's unfinished tasks are left running"; only an idle loop is
   closed and its roots destroyed. The drain case checks the warning (deterministic: its mutant, the old destroy,
   misses the line). Diagnostic kept (Xapiand:bedf01221): the test node prints a SIGABRT's sender and stack.
   For the lead and strata: a group awaiter whose frame is destroyed stays linked (chore_awaiter, quorum and read
   waiters have no destructor that unlinks); destroy-safe awaiters there would make destroy_roots safe everywhere.
5. Replay check 8bf61b: recovery correct, the file-size line missing. The harness now prints the failed start's
   log end on that miss (Xapiand:bedf01221); d250e2 shows what the node logs under `ulimit -f`.
6. R1: the mutant is 261123-232529-f93da2 (killed, exit 1), not 4ee4de (127, never ran).
7. Merged origin/master (Xapiand:a740b6751, clean: the docs site engine, pages.yml; master's site files taken).
8. Session 12 item 5's verdict (loop-yield probes, 3 runs each): both sides moved. Merge base 7ff092: in chores
   168.9-174.5 ms, on the loop 2,752-3,076 ms (0.057-0.062). Head 4fae12: in chores 227.2-273.2 ms, on the loop
   1,546-2,005 ms (0.135-0.147). The denominator shrank 1.5-1.8x (the group applies every write on its chore, so
   "on the loop" no longer applies there) and the numerator grew 1.3-1.6x (+55-100 ms per 2,000 documents, the loop
   proposing each command, ~30-50 us a document). CI's 0.21-0.22 is the same shift on slower runners; the bound
   was not changed. For the lead: the ratio's denominator no longer measures what it did; an absolute loop budget
   per document would state the claim.

Session 12 (2026-11-23), task `levels` and R1 on the quest branch (lead's assignment), then the lead's two messages:

1. R1 (Xapiand:c159d38a5): the journal host's PosixIO takes `engine::strata::posix_io_options()`; test_shard_group
   `test_the_journal_io_takes_the_io_options` (full_flush_ordering both ways, the barrier warning, a group-writable
   directory the defaults refuse). Mutant: the host built with `PosixIO(directory)`.
2. Levels (Xapiand:650bb198b, tests Xapiand:ea9598e6d, docs and harness Xapiand:e5d8c3a6d): ack=quorum|none,
   refresh=false|wait_for|true, read=prefix|linearizable (and ?volatile), ?commit = ack=quorum&refresh=true, 400 on
   unknown or contradicting levels; RESTORE takes ack. The group's apply marks the engine through
   `ShardApplication::applied` (on the chore, before the answer, at either level); Core's dirty/pending are atomic and
   cleared before their work. test_node: `test_ack_quorum_answers_once_applied`,
   `test_ack_none_answers_at_placement_and_linearizable_reads_wait_for_it`, `test_refresh_levels_and_commit`,
   `test_a_restore_at_ack_none_reports_no_versions`. Docs: basics.md "Visibility and Durability" with E2E examples
   per level and a 400; manipulation, architecture, get and search pages, README, options, changelog;
   journal-durability.md "Levels". harness/power_loss_check.sh (D14 tier, `--later`) replaces wal_recovery_check.sh.
3. Lead msg (d17a82, the abandon case): reproduced at c4d06c80d by probe 261123-160215-a97282 (2 of the first 10
   runs, host load ~59). Verdict: a real race in the shutdown, not the bound. join() cleared the registry's control
   before draining the control loop, so an autocommit timer firing in the drain ran synchronously (run_detached's
   sync_wait) on the draining thread; an earlier autocommit of the same index, a task of that loop, held the turn
   or the commit flight and could only resume on that thread. The child's log: "Autocommit of 'acked' failed after
   30s: The index is busy: its turn did not come in time", then Stop 1 thirty seconds late (the turn deadline); the
   second failure never reached Stop 1 (the commit flight has no deadline). Fix (Xapiand:6686f9ffc): the control
   stays set until its scheduler closes (Stop 2); the names' build stops up front (`stop_names`); finish() leaves
   the control to its owner. Test: `test_an_autocommit_during_the_shutdown_drain_runs_on_the_control_loop` (a held
   group chore under an autocommit, a second one fired into the drain; the stop must end in its bound). Mutant: join
   clears the control before the drain again.
4. Lead msg (576cc8, the wildcard over the cap hung at 91113fb2f): not reproduced (d17a82 passed it); no mechanism
   found by reading. The watchdog now samples every thread's stack on a hang (Xapiand:03637fc50), so a recurrence
   names its frames. The 10x test_node job at the head (261123-165247-a8e7ab) is the check. The CI failures the lead
   lists at :2255, :2620 and :737 were fixed before this session (lead's note).
5. Lead msg (test_loop_yield.cc:693 on CI, 3 runs): CI's numbers on this quest are 0.21 and 0.22 (142.7/674.7,
   109.1/490.5 ms; bound 0.2, master's comment 0.06-0.09). Hypothesis: the denominator shrank, not the numerator
   grew: through the group every write applies on its group chore, so "prepared and written on the loop" no longer
   writes on the loop. Probes 261123-161429-7ff092 (merge base) and 261123-165249-4fae12 (head; 5784fc failed to
   compile, the write_levels declaration) measure both sides. No verdict until they land.
6. Out of set: the get and search API pages (`volatile`) were edited and reverted (Blocked on 10).
7. Lead msg (crash gate 261123-165000-bd1f5e at 0e256600e: "a search batch pins its indexes together: hung"). The
   watchdog's sample (its log, line 69395 on) shows no deadlock and no pin wait: the 16 pool workers are parked,
   the test thread waits in sync_wait at test_node.cc:1584 (the 24-index setup's `shard.commit()`, not the batch
   search), the control and HTTP loops idle in kevent, and the journal host's loop thread (ShardGroups::Driven,
   InlineCompletion) is in PosixIO::sync_files for all 41 samples: 27 in fsync, 14 in fcntl at posix.h:244, the
   durable mode's F_FULLFSYNC per device. Verdict so far: not a pin order or a pin held across a turn; every
   group's IO completes inline on that one thread, so the setup's 24 creates, writes and checkpoints serialize
   their durable syncs, each a full device flush on macOS, and under the host's disk load (other jobs' builds and
   syncs) they took past the 40 s watchdog. The wildcard case (30 creates, each ?commit a checkpoint at 91113fb2f,
   120 s watchdog) is the same shape. Probe 261123-174805-f1b77c times the case against the volume's
   fsync+F_FULLFSYNC latency in the same job, so the ratio counts the full flushes it pays; that decides between
   fewer syncs per create/write/checkpoint (the journal host's batching, strata's side: a finding for the lead) and
   the watchdog's bound. test_node 30x at the head: 261123-174823-c5379e (--long, stops at the first failure).

Session 11 (2026-11-23), the evidence set at the head (lead: nothing else unless a job fails):

1. 261123-142137-528ce7 and 261123-142155-d17a82 had not run (109 jobs queued ahead): no verdict yet on the count
   must-fail or on whether test_node's wildcard hang was load.
2. A4 had no test: none read right after a write through the group. Xapiand:7ab94031a adds test_pool
   `test_a_lone_voter_reads_its_writes` (three writes through the group, each followed by a get-by-id and a search
   that must see it, no commit); its mutant drops `core.dirty = true;` in `Shard::applied_by_group`, so a read serves
   the last refresh (ruling 31).
3. The full set at 7ab94031a, fresh submits with no --after (State): every mutant pattern checked to match at the
   head with git grep; the A3 term-staging mutant's line moved (`if (open.term < e.term)` is now
   `if (chunked_term(id) < e.term)` in effects), the rest are the earlier commands. No job uses a ref override.

Session 10 (2026-11-23), the lead's step (Blocked on 6 and 7, file set widened to data.* and agent-codebase.md):

1. Xapiand:09a223a4e: Data::flush copied its locators and rebuilt `serialized` in place, so after a flush each
   locator viewed the bytes it replaced (in place, while they were overwritten; freed once the string grew) or a
   pending update's holder the flush then cleared. Now the flush builds its bytes apart and `parse()`s them (feed's
   body): every locator views the Data's own bytes. Second hole, same class: a Locator moved or copied (pending's
   vector growing) kept `raw` on the source's holder, which for a short (SSO) holder is inside the moved-from
   object; Locator's copy and move now view their own holder when the source viewed its own. place_stored's
   call-site reparse (Xapiand:3fc80367f) goes: one mechanism, the flush. Tests (test_shard_group):
   `test_a_flush_leaves_its_locators_on_its_bytes` (every locator's raw inside serialize() after each flush, the
   blob bytes read back, a reparse then replace, a copy), `test_place_stored_keeps_the_other_locators`.
2. Xapiand:91113fb2f: docs/dev/agent-codebase.md describes the journal: the durability row (shard_group, command,
   the group's `app.vol-N` file set), two invariants (apply is a function of entry and starting state; prepare
   writes nothing), two traps (durable at the journal's ack, commit is the checkpoint; a locator views its Data's
   bytes). `git grep -i wal` on it is empty; `no-wal` greps src only and is unaffected.
3. Xapiand:ecf684865 (lead 2026-11-23: remove the storage library in this quest): its FetchContent block, the
   STORAGE_IO_HEADER/STORAGE_TRACE_HEADER defines, src/xapiand_storage_{io,trace}.h, the storage.h includes in
   manager.cc and database/utils.cc (outside the set; the include would not compile without the library), the
   async allowlist row. DATA_STORAGE (option, config.h.in define, the ifdefs in handler, pool, shard,
   search_views) goes; the stored-blob paths it gated always build. README and building.md option rows,
   dependencies.md (row and tree), testing.md's storage paragraph and the zstd comments in CMakeLists stop naming
   it. cmake/ and .github/ name neither. Proof: build job 261123-113717-eb5101.
4. Xapiand:caa9a372e (lead: ARCHITECTURE.md added to the set; docs only, no job): the storage-engine row and section
   describe the journal (host threads, prepare/propose, checkpoint as commit, ShardApplication, file-set blobs);
   the runtime diagram's D2 source and the background-work rule name the group loop and chore threads. Each
   storage/WAL citation: extraction row re-pointed (Kronuz/storage, done, unused); bug entries dropped: inert
   footers (storage.h:200-227), fsync barrier (storage.h:863-890), async WAL fsync (wal.cc:67), WAL replay's blob
   re-push (shard.cc:1242); LZ4-digest entry re-pointed from "WAL lines" to the LZ4 blobs data.cc still reads; gems:
   format-as-type (storage.h:232) replaced by apply-as-a-function, WAL slot array (wal.h:63) dropped. No WAL,
   storage.h or fsyncher mention is left; none was a dated record. The stale scheduler/threadpool text stays (lead's
   separate docs item).
5. assets/runtime.svg (lead added it to the set): re-rendered with `d2 --theme 0 --pad 20` from ARCHITECTURE.md's D2
   source, which gains three edges (shards propose to the group loop, committed entries to the chore threads, apply
   to the file sets) and the label "journal host"; checked from a PNG render: no WAL writers or fsyncher, nothing
   clipped. Docs only, no job.
6. CI 45251581577 at 41dcff35d failed at configure (zstd's `uninstall` target clashes with Xapiand's). Cause: the
   storage library ran FetchContent_MakeAvailable(compressors) early, before strata set COMPRESSORS_ZSTD_PINNED ON,
   so compressors took the system zstd; with it gone, prospect -> strata processes compressors first, pinned, and
   compressors adds facebook/zstd's CMake project. Fix Xapiand:6bb073ca9: Xapiand's own zstd find moves before
   prospect; compressors takes a found ZSTD_LIBRARY/ZSTD_INCLUDE_DIR even when pinned, so one system zstd links, as
   on master. Not an `if(NOT TARGET uninstall)` guard: that would keep two zstd copies and give `uninstall` to zstd.
   Why local jobs missed it: none configured fresh at ecf684865 (eb5101 was still queued), and the slot's reused
   `build` dir caches ZSTD_LIBRARY/ZSTD_INCLUDE_DIR from earlier configures, which hides the fetch; CI configures
   fresh. The new jobs configure in a removed `build-fresh`. Master merged: Xapiand:d322cc9b6 (clean).
7. 261123-112555-9f402f (test_pool under ASan at a22e1537f, before any fix) passed its test (log: "100% tests passed"):
   the UAF CI saw does not reproduce in test_pool under macOS ASan, so it is not fails-without evidence. The flush fix's
   must-fail proof is a11289 (test_shard_group's deterministic views check, which needs no ASan to fail).
8. CI at 90f2688fa: test_shard_group fails `REQUIRE_EQ(data.size(), 3u)` (line 322) on all three jobs. Neither the
   master merge nor the zstd change: my own test from 09a223a4e counted wrong. A new Data holds the empty main object
   (DATABASE_DATA_MAP), so three updates leave four locators; its first CI run was this one (earlier pushes cancelled
   CI before tests). Fix Xapiand:c4d06c80d: the count is 4. Attribution: 528ce7 fails at line 322 at 09a223a4e itself.
   The fix is to the test's expectation, so CI at c4d06c80d is its proof.
9. test_node 576cc8 at 91113fb2f failed after 530 s: "a node over the cap answers a wildcard: hung (schemas: 192
   loads, 34 joined, 0 waiting; settings: 148 loads, 6 joined, 0 waiting)", host load ~33; CI's test_node passed at
   90f2688fa, and the flush fix does not touch the schema/settings load path. Rerun once at c4d06c80d: d17a82. If it
   passes, the hang was load (the test's own deadline under a host 4x oversubscribed); if it hangs again, it is a bug
   to chase.
Observed, not changed: strata pins zstd and lz4 so segment bytes match across platforms; in Xapiand the zstd pin is
defeated by the system zstd (as on master, where the storage library did the same). For the lead and strata before
replicas compare segment bytes.
Observed, not changed: no `Storage<>` is instantiated in src any more (manager.cc and database/utils.cc include
storage.h for nothing or its magic constants; xapiand_storage_io.h is unused), yet CMakeLists still fetches the
storage library and has DATA_STORAGE. For the lead: removing it is the WAL removal's tail (Proposed tasks).

Session 9 (2026-11-23), CI at Xapiand:a22e1537f red on 6 jobs (lead's log ~/.scratch/xapiand/lead/jrw-ci.log), Xapiand:3fc80367f:

1. The two asserts (pool.cc:358 take_turn, pool.cc:101 ChoreAwaiter) are the expected output of test_pool's death
   tests (test_a_task_holds_one_turn_at_most, test_a_chore_needs_a_scheduler print "ok" right after), on master too.
   The jobs of session 8 were RelWithDebInfo with -DASSERTS=ON, as every job of mine is.
2. heap-use-after-free in Locator::serialize (TSan, ASan, and the E2E's 616 failures; the plain E2E's "POSITIVE_INTEGER
   is not iterable" on UPDATE /assets/sample is the same corruption): place_stored set the codecs on the locators of a
   Data it had flushed and flushed again; Data::flush rebuilds `serialized` while its locators still view the old
   bytes, so the second flush read freed memory. Present since d2 (CI at Xapiand:be7dc926c shows the same 616), not
   from session 8. Fix: the codecs are set on the flushed bytes parsed again. test_pool.cc:675 (the kill -9 child
   ended by ASan's abort, not SIGKILL) is this too. Proof: ASan test_pool at head and at a22e1537f (State).
3. test_loop_yield.cc:693 (chores 142.7 ms vs on-loop 674.7 ms, 0.21 > 0.2): already at the edge before session 8 (CI
   at be7dc926c: 171.0/934.2 = 0.18; local 0.13; 0.06-0.09 in the WAL era). Cause: the group's RESTORE slice encoded
   every batch command on the HTTP loop (split_command inside propose copies each document's bytes). The chore now
   encodes each command's single entry and drops the writes' bytes; the loop proposes the entry. The targeted job
   prints the new ratio; e33085 gives this machine's pre-fix numbers.
4. origin/master merged (Xapiand:819a107b7, clean; pages.yml, AGENTS.md, agent docs).
Not the cause of any failure: the slow-commit test's chore hold and the written_key != shard_did check (the failures
above reproduce at be7dc926c, before both).

Session 8 (2026-11-23), fix round on crash check 261123-083737-64aa68 (Xapiand:0aeec8fc9, 3 of 90 failed), Xapiand:693c91e68:

1. test_pool fenced (code): the group's checkpoint threw strata::index_fenced untranslated, so error_kind() let it
   escape and the endpoint never went stale. commit_group now translates through `engine::strata::Shard::rethrow_failure`
   (rethrow_translated with the Core: errc::database, Core fenced), as the engine commit did.
2. test_node dirty (:2255), revision (:2620) and the stop order (child abort: do_commit's `current == prior + 1` at
   shard.cc:474, node_order.log in the slot) (code): the checkpoint commits the index on the group's chore and the
   engine's Core kept pending/dirty and its last own revision. `checkpoint(group, at)` now runs `at` on the chore via
   read_applied after it commits; commit_group passes `Shard::checkpointed()` (nothing owed, note() the manifest) and
   sets the endpoint's revision.
3. test_node slow commit (:737) (test): it gated the endpoint's flush chore, where the WAL-era commit ran; the group's
   checkpoint never uses it, so nothing held the commit. The test now holds the group's chore (SchedulerHold, a
   post_call that blocks the chore thread), where the checkpoint captures: same claim (the control loop stays free
   during a 400 ms commit), on the commit's real path. GateChores and A::set_chores/take_chores went with it.
4. test_loop_yield deferred==3 (code): "0 writes deferred" is deterministic, not load: restore_slice missed a write
   only when its key was gone, so k7 erased and written again on the same shard (new docid) was replaced by key. B1
   (and the engine's must_exist replace by docid) misses a document that left its placed docid; the check is now
   `written_key(term) != shard_did` (strata's replace_key keeps the key's docid, so repeats in a slice still match).
   Job 261123-101801-e33085 shows the pre-fix result at a quiet time, as the lead asked.
Job churn: the full set I queued at 693c91e68 (crash 1c2ab1, no-wal 8f25a0, reruns 8bc6da..d7f696, mutants
c0dc0e..4ba74e, 4f0aa1) was cancelled on the lead's word (targeted jobs first, the set once at the final commit). The
reruns' supersession had already cancelled 1d9f09 and 58484d at 0aeec8fc9 and the old mutants.
Observed, not changed: a later call on a group fenced earlier throws strata::group::group_fenced, which
rethrow_translated maps to errc::internal (not database), so that endpoint would not go stale by it; the first failure
already marks it stale. For the durability review.

Session 7, round 2: the mechanism notes, testing.md, the three comments say journal; status and audit records keep their wording with wal-replay.md links repointed. The git grep leaves only changelog.md:284 and options.md:418 (they state the log's removal; changelog:70 now says journal).

Session 7 (2026-11-23), docs and harness cleanup (item 5): deleted reference-guide/wal.md and mechanisms/wal-replay.md (the sidebar is generated from the content, no entry to remove); removed the WAL verb row from api.md; reworded about, basics (points at journal-durability.md), manipulation, backups, dependencies, internals, tests/coverage.md; stored_stream.sh counts `*vol-[0-9]*` files instead of `docdata.*`; search_views.h has no WAL text. Remaining hits (outside the file set, left): docs/dev/{cr1-replay,docs-site-audit,testing}.md, docs/mechanisms/{control-plane,engine-write-path,seam-fields,strata-engine}.md, docs/status/{cr1-fix,phase7c-gate-prep,phase7c-x6}.md (cr1-replay.md and cr1-fix.md link the deleted wal-replay.md), src/engine/document.h:151, src/engine/strata/document.h:149, tests/fuzz_strata_document.cc:23 (comments). In-set hits left on purpose: changelog.md:70,284 and options.md:418 (they state the log's removal).

Session 6 (2026-11-23), the lead's fix round.

1. Xapiand:3d20661bd: crash check 316fa1 did not compile: shard.cc still built a DataStorage whose header pieces
   (UUID, the Storage base) came through the deleted wal.h. Nothing in production reaches it (every index is a group),
   so the data storage goes: DataStorage and its headers, StoredUpload, upload_begin, storage_push_blobs/commit, the
   `\x00` locator metadata read, the chores parameter of do_add/do_replace/do_replace_term/do_commit; the handler's
   upload is the group's chunked command only (UploadedBlob is size, shard, upload). A pool with no journal refuses a
   stored blob (ruling 21). test_blob_io_runs_in_chores reads through the group; new test_a_stored_blob_needs_the_group.
2. Same commit, A9: test_a_reopen_aborts_an_upload_left_open failed (3847bb) because it wrote without a commit, and the
   sweep closes only a committed index (strata catalog run_close: close_ready() needs nothing uncommitted), so the slot
   never freed and the "reopen" pinned the same open entry, where the upload's final applied. The test commits before
   the close now. The A9 fix-3 mutant (599a5c) shows whether the ready point's abort is what refuses the final.
3. Xapiand:f6d8f2c65: the stale WAL text in the set (Blocked on 4 answered): config.h.in, log.h, metrics (wal errors,
   fsync gauges), README, building/options/architecture/changelog pages, strata_write_check.sh (no :wal, no WAL log
   lines, replaycrash scenario gone: its hook was the WAL's), engine_crash_check.sh (GRACE 0, ruling 23),
   graceful_shutdown_check.sh, cluster/checklist.py, coverage_e2e_server.py (no WAL verb rows), one_id_mutants.sh
   (no-replay gone), test comments, test_quarantine's names. profile_bulk.sh keeps its WAL row (ruling 22).
4. Xapiand:0aeec8fc9: origin/master merged (campaign files only). AGENTS.md not edited: agents-md has not landed.

Session 5 (2026-11-23), step e (A8). 3847bb was still running, so e went first.

1. Xapiand:9068ca7ee: the WAL (wal.*), the fsyncher, the scheduler library (FetchContent block, debouncer, stash,
   both trace headers, debounce_or_run and its test), DB_DISABLE_WAL, DB_SYNCHRONOUS_WAL, the WAL HTTP verb, the
   `wal` argument of every write and commit, `--writers` and `--wal-writer-cache-size` (now in
   removed_options_check.sh), the info keys, the fsync metrics sets and Stops 4-5 are gone. logger_fwd.h keeps
   only L_NOTHING and the L_EXC stub. test_pool's WAL tests go (the log's order is apply order by construction)
   or read the journal (FaultyIndexes keeps one; cold pin, fenced, id counter). async allowlist: the WAL rule
   and rows go, shard_group.cc's blocking sites are classified (CI 45203739416's 8 unclassified hits);
   `async_check --strict --owed` locally: 0 unclassified. run_all.sh's recovery layer runs journal_replay_check.sh.
2. Xapiand:c0dc4160d: master merged (no conflicts; strata pin stays 00eb018, which contains master's e3edb426).
3. Not done: Host::delete_group on index DELETE (Blocked on 2).

Session 4 (2026-11-23): the six jobs of session 3 were still queued (18 ahead), so the code stays unbuilt; d2's job
at head builds c, d1 and d2 together (it superseded d1's own job and mutant, resubmitted at d2).

1. Xapiand:4a117e647: the journal at `.xapiand/journal` (the lead's ruling, Blocked on 1 answered).
2. Xapiand:ed1ded9c6 (d2): no data storage with a group; `stage_blobs` compresses new stored blobs at prepare into
   CommandBlobs with placeholders at volume 0 (codec and size), `place_stored` sets them at apply; uploads stream
   as chunk entries (`upload_stored` through `next_chunked_id`/`propose_upload`), the document is the upload's
   final, proposed once (`ChunkedUpload::finished`), aborted on any failure before it; a read opens
   `app.vol-N` through the slot's directory (`read_stored`). `landed` refuses an accepted verdict with no docid
   (a final whose upload was retired). Test: test_pool `test_stored_blobs_survive_a_kill`.
3. Xapiand:6c44375c5 (d3): apply_slice with a group is `restore_slice`: a chore prepares the slice (do_replace_term
   leaves a CommandWrite, `batched`), RestoreBatcher packs, each command proposed at quorum in order, verdict ->
   `restore_results`. apply_slice/SliceWrite stay until e (the no-group path and test_loop_yield's seams use them).
   Test: `test_a_restore_slice_is_batch_commands`.
4. Xapiand:1fe793170 (d4, A9 fix 3): a request's commit with a group is `commit_group` (checkpoint on the loop,
   under the turn); the hook stays a plain strata commit. The id counter rides the write's command as set_metadata
   (`id_high_logged_`), a RESTORE slice logs it after its batches; open reads it on the chore. Ready point:
   `catch_up` reads state.open on the chore, `abort_open` proposes aborts. Tests: `test_a_commit_is_the_groups_checkpoint`,
   `test_the_id_counter_is_in_the_log`, `test_a_reopen_aborts_an_upload_left_open`.
Job churn: earlier sets (b8aa54, a03d73 and their dependents; session 3's queued c/d1/A9 jobs) were cancelled
unrun and replaced by the one set at 1fe793170 (lead: one set per head, no resubmitting per push).

Session 3 (2026-11-23): merged origin/master (Xapiand:f1944bc00, clean); strata pinned at main 00eb018d (group-catalog,
D169 (a)); chunks and restore tasks were merged by the lead (restore's three failed probes are by design, not evidence).
Built, each its own commit, none compiled locally (jobs only):

1. Xapiand:4fe9f1e91: the pin; test_group_chunk_group.cc lends its group the index (group::open_index), as a slot does.
2. Xapiand:e2f2bc9cf (A9 fixes 1-2, shard_group.cc): a chunk or final placed outside its id's term applies nothing;
   `Open::term` goes (the id names it; state bytes version 2); an abort (a final naming no blob of its upload)
   retires the upload volume. chunked_id/chunked_term move to command.h. Tests: test_shard_group
   (`test_entries_outside_their_term_apply_nothing`, `test_abort_retires_its_upload_volume`), test_group_chunk
   (the abort frees its volume).
3. Xapiand:089de4ba9 (step c): `ShardGroups` (shard_group.*): journal host over `.journal` in the data directory, the
   group loop's thread, two chore threads; DatabasePool::set_journal (the manager calls it); ShardEndpoint opens its
   `ShardGroup` last in open_blocking(directory, index) and the catalog's close_payload closes it, keeping the closed
   record. Test: test_pool `test_an_index_is_a_lone_voter_group`.
4. Xapiand:ffd0f8dd9 (step d1): `ShardEndpoint::write_document` prepares on the engine, `command_write`, proposes on the
   loop at quorum through `propose_command` (ids from the loop's term), `engine::strata::Shard::landed` checks the
   verdict and owes a refresh; set_metadata is a command; no WAL line or replay with a group; the open waits on
   `ShardGroup::catch_up` (a read_applied barrier). Test: test_pool `test_an_acknowledged_write_survives_a_kill`
   (fork, write, SIGKILL with no commit, reopen reads it); mutant 3e5477 routes writes to the engine.

Earlier sessions:

Session 2 (2026-11-21): the lead answered Blocked on 1-4 (files widened; D169 option (a), strata quest
group-catalog builds it; this quest lands after it). Merged master (cf23fbda7). Building what does not
need group-catalog:

1. CMakeLists.txt: strata pinned at main efa5214b, `strata::group` linked into XAPIAND_OBJ, faultlab
   e3fcfe3 fetched for test_pool (faultfs moved to faultlab, `kronuz::faultlab`).
2. Engine split: `engine::Shard::prepare(op, did, key, doc, args)` and `apply(prepared, doc)`; add/
   replace/replace_by_key/erase/erase_by_key are base-class wrappers over the two.
   `engine::strata::transaction_of(prepared, doc, field_table)` builds the transaction at apply against
   the applying index's table; apply passes prepare's expectation to strata (`expected_version`), so a
   stale prepare is a definite version_conflict on every replica. Test tests/test_group_prepare.cc.
3. `src/database/command.{h,cc}`: the D3 command (writes: op, did, key, expected, document bytes,
   blobs; batch for D17; schema version) and D12's chunk and final entries, `split_command`,
   `resolve_command`. Test tests/test_group_command.cc.

## Next

1. Read the six jobs and CI at ffd0f8dd9; the code was written without a build, so fix compile errors first (the
   step-c job at 089de4ba9 separates c's errors from d1's). For each job, check the log shows a complete build and
   the named test.
2. d2, blobs: storage_push_blobs becomes CommandBlob records compressed at prepare with placeholder locators;
   storage_get_stored reads `app.vol-N` through the group's file set (on the chore, or a reader of the file);
   upload_stored/index_uploaded take propose_upload/propose_upload_final (the handler.cc SEAMs); DataStorage and
   StoredUpload go. A2's crash evidence (blob_crash_check.sh, the `crash` check) on that build.
3. d3, RESTORE: DocIndexer::apply_shards feeds RestoreBatcher through RestorePropose (each command once the
   previous is placed); Shard::apply_slice and SliceWrite go; journal_replay_check.sh on that build (SETTLE=0).
4. d4: commit() awaits group.checkpoint() on the loop from a request (never from a commit hook); id_high moves into
   the log (a set_metadata command at issue, or ShardGroupState).
5. A9 fix 3: after catch_up in open_blocking, the leader proposes abort_entry(id) for each id in state().open; test:
   a close with an upload in flight resumes its term at reopen and the abort retires it.
6. e: written (session 5); Host::delete_group on index DELETE waits on Blocked on 2. Read the step-e jobs (State).

Done (session 2's plan, kept for reference):
2. c. Build against group-catalog (FETCHCONTENT_SOURCE_DIR_STRATA=/Users/kronuz/code/Kronuz/strata-wt/
   xapiand/group-catalog, strata:096dbd04): its `group(loop&, config, jn::IO& directory, index_type& index,
   App&, chore)` borrows the slot's index. One `strata::group::loop` per reactor loop and one
   `journal::Host` per data directory in manager.* (restore task holds manager.*: coordinate, or the
   lead moves the loop/host setup into pool.*); the catalog slot constructs
   `group<ShardApplication>` over its index and IO.
3. d. ShardEndpoint writes: prepare under the turn (engine prepare), the blobs moved from
   storage_push_blobs into CommandBlob records (compressed at prepare, placeholder locators), then
   `split_command` and `propose`; ack levels via `propose(level)`; the blob read path for app. volumes
   (storage_get_stored); commit() posts a checkpoint to the group's chore (msg D169 revised: never awaited
   from a commit hook); open_blocking's WAL replay goes. id_high: from the index at open, or into
   ShardGroupState (decide at d).
4. e. Delete the WAL (wal.*, fsyncher, DB_DISABLE_WAL / DB_SYNCHRONOUS_WAL and the four tests' flag,
   options), `no-wal` check; merge racing-put-409 and resolve it in prepare_write's replace case.

## Messages

- Lead 2026-11-21 (group-catalog's contract change, D169 revised): the catalog's commit hook never
  sync-waits on `group.checkpoint()` (commit hooks run inside chore_scope; the group's chore takes the
  index's write lease, which waits while the chore bit is held: deadlock on any lane). The commit stays
  a plain strata commit; the group's checkpoint runs at close through the new catalog hook
  `close_payload`, outside the lease, before index.close(). So step d: `commit()` posts a checkpoint to
  the group's chore (periodic or explicit), never awaited from a commit hook. Push once per round of
  code, when only jobs remain (a push cancels the branch's CI run).

- Lead 2026-11-21 (racing-put-409 lands first in src/engine/strata/shard.cc, Xapiand:01ec326b6 and
  Xapiand:cc8c27362): a replace whose key sits at another docid is `version_conflict(found->second)`, not
  `invalid`. Not applied ahead (test_engine_strata.cc, outside my set, still expects `invalid` until it lands):
  when master brings it, the merge conflicts in Shard::replace; resolve it in prepare_write's replace case.

## Proposed tasks (posted to the lead)

The quest's own table holds: `core` (me) first; then `chunks` (handler.*, tests/test_group_chunk*,
harness/blob_crash_check.sh) and `restore` (manager.*, tests/test_wal_replay.cc, the three WAL
harness scripts, profile_bulk.sh) in parallel, disjoint files; then `levels` (opts.*,
search_views.cc, the request tests, power_loss_check.sh, run_all.sh, docs). One addition, in
strata: `group-catalog` (item 4), which `core` step c waits on.

## Blocked on

10. (2026-11-23, done: the lead added both pages to the set; Xapiand:074e8cc5c puts the text back) Outside the file set: docs/src/content/docs/reference-guide/documents/get-api.md ("Volatile") and
    reference-guide/search/search-api.md (the `volatile` row) still say `volatile` opens the index for writing. D4: it
    means read=linearizable. Edited, then reverted (Xapiand:6d6921068) to keep the set. Recommend: add both and I
    apply the reverted text (get: "`read=linearizable` (or `volatile`, which means it) waits until every write the node
    answered before the get has applied, an `ack=none` write included; the default, `read=prefix`, reads what has
    applied", linking basics#visibility-and-durability; search: the same row for `read`, `volatile`).

6. (2026-11-23) Outside the file set, left: docs/dev/agent-codebase.md (new from master's agents-md) still describes
   the WAL (lines 14, 71-72, 76, 80: `src/database/wal.*`, WAL replay, WAL durability). Recommend: add it to the set
   and I reword it to the journal, as item 5's files were.
7. (2026-11-23) Outside the file set: Data::flush (src/database/data.cc) leaves its locators' `raw` views in the bytes
   it replaced, the root of item 2 of session 9; place_stored no longer touches locators after a flush, but any caller
   that does is exposed. Recommend: add src/database/data.* and have flush feed() its new bytes at its end (one
   parse), with a unit test that flushes twice under ASan. Until then, nothing in the quest depends on it.

9. (2026-11-23, done in session 10 item 5) Outside the file set: assets/runtime.svg is rendered from ARCHITECTURE.md's D2 source, which now
   shows the journal threads; the SVG still shows the WAL writers and fsyncher. Recommend: add it and I re-render
   (`d2 --theme 0 --pad 20 <source>.d2 assets/runtime.svg`), or the lead's docs item takes it.
8. (2026-11-23, done in session 10 item 4 after the lead widened the set) Outside the file set: ARCHITECTURE.md still describes the storage library and the WAL as current
   (the Storage engine row and section, `storage.h` citations, the "Haystack-style store" extraction row, three
   storage/WAL bug entries, the format-as-type gem; 17 WAL mentions). It reads as a dated close-read review, but
   its subsystem table and sections are descriptive. Recommend: widen the set and I rewrite the storage-engine row
   and section to the journal and drop the removed code's bug entries, or a docs quest refreshes the whole file
   (its scheduler and threadpool text is stale too, as is dependencies.md's tree on master).

Items 6 and 7 are done (session 10). Earlier items are answered below.

Lead rulings, 2026-11-23:
- 2: option (ii). Index DELETE is its own quest, index-delete (after strata-journal-pin bumps strata's journal pin
  past journal:131eb2e, and after this quest). 501 is today's behavior on master, not a regression here.
- 3 and 4: the file set now covers the three deleted files and every file in item 4. Removing the WAL means
  removing it everywhere (code, options, metrics, docs, harness); do item 4 in this quest. AGENTS.md is shared
  with agents-md, which lands first: merge master before editing it.
- 5 (2026-11-23): the set now covers every file item 5 lists, plus docs/astro.config.mjs and docs/src/sidebar*
  for the page's sidebar entry. Delete wal.md and wal-replay.md; fix the rest in this quest.

## Evidence

Jobs prove criteria by their tags; paste `campaign brief`'s generated table here when they finish.

| Criterion | Evidence |
|---|---|
| A1 | 261123-165103-2b225f; 261123-165150-46d736; 261123-165229-26f9f6; 261123-165235-1c28ca; no-wal 261123-165252-228c39; crash 261123-165252-fef4e8; fails without: 261123-165135-36e01f, 261123-165137-8ec228, 261123-165140-99c0aa, 261123-165142-9e1f71, 261123-165145-a60381, 261123-165147-e4c73e, 261123-165232-41ee12 |
| A2 | 261123-165106-195af5; fails without: 261123-165109-560112, 261123-165112-923dcd |
| A3 | 261123-165114-9e834c; 261123-165117-9d42ae; fails without: 261123-165120-541aae, 261123-165123-cf493c, 261123-165126-61010f |
| A4 | 261123-165129-548f8f; 261123-165103-2b225f; fails without: 261123-165132-60295c |
| A5 | ci:Functional E2E (docs); 261123-165152-8bf61b; 261123-165210-4f05e9; fails without: 261123-165154-d1a0f8, 261123-165157-ded424, 261123-165213-4ee4de, 261123-165215-2c8a56, 261123-165217-7c699d, 261123-165219-0f06b3, 261123-165221-583df2, 261123-165223-998438, 261123-165225-1c5d88, 261123-165227-d0f01f |
| A6 | D113 (owner: Q1 (a), default ack quorum, bulk loaders choose none) and D142 (owner: Q6 as written, ?commit = ack=quorum&refresh=true; Q-F27 (a), read-your-writes served from the leader's replica); the code ships both defaults, A4's test covers Q-F27 |
| A7 | docs/mechanisms/journal-durability.md: Every shard is a strata::group: a lone voter at rf = 1. The group's journal is the shard's only durability path |
| A8 | 261123-165208-588fd6 |
| A9 | 261123-165103-2b225f; fails without: 261123-165200-4ebfb3, 261123-165202-26b0dd, 261123-165205-dd6ea0 |

## Result

<When ready for review.>

## Rulings I made

Drafts, applied when the code lands; each names the contract it shapes (format-freeze reviews them).

1. Blob volumes are the group's file-set files (`app.` prefix) in a positional format of their
   own, not the storage library's block framing: D13 converts no pre-release directory, and
   `Storage::write` appends where the file ends, which D3's retry-from-start forbids
   (`append-on-retry`). Contract `xapiand:volume-file-set`.
2. A blob's compressed bytes are made at prepare and carried in the command; apply writes them
   verbatim. Then a locator's offset is a function of the log even across replicas built with
   another zstd. Contract `xapiand:command`.
3. The command carries `engine::Document::serialize()` bytes as D3 says; the strata transaction
   is built at apply against the voter's own field table, so a field declared by an earlier entry
   resolves the same on every voter.
4. The task pin moves to strata's own (95798a02): strata::group's journal and consensus headers need
   task's blocking_scheduler, which Xapiand's 18eb5167 lacks. A consequence of the strata bump the lead
   assigned (CMakeLists.txt is in the set); CI tells whether anything else in Xapiand depended on 18eb5167.
5. faultlab is fetched for test_pool only, at strata's e3fcfe3 (strata declares it only as top level).
6. `engine::PreparedWrite` (not `Prepared`: engine/search.cc has a `Prepared` query class). Its
   `expected` is always set by prepare for replace and replace_key (the leader's expectation, D3), the
   key's version for erase_key, and none for erase by id (strata checks versions only by key; prepare
   checks it). `WriteOp::none` and `replayed` stay (revised, session 5): they serve any versioned document's replay, and their
   tests are in test_engine_strata.cc outside my set; removing them is a follow-up (Blocked on 4).
7. A batch's transaction declares a new field once: a later write of it names the earlier declaration
   (the table keeps the first document's kind, as sequential transactions would).
8. Volumes rotate past 32 GiB (`VOLUME_BYTES`, master's data-storage volume bound, storage.h); locators keep
   byte offsets. Chunked-command ids must be unique across the group's log (the proposer puts its term in
   the high bits); a chunk of an id open in a lower term is dropped, and its final writes nothing.
9. The command codec is LEB128 over the kinds command/chunk/final with a version byte (pre-release bytes,
   contract `xapiand:command`/`xapiand:chunked-command`, format-freeze reviews them); `split_command`
   stages the largest in-place bytes first until the final fits.
10. The journal's IO completes inline (InlineCompletion) on the group loop's own thread, never a reactor's:
    correctness first; T11 measures whether the loop's syncs sit on the write's critical path.
11. A group's id is the first 8 bytes of its index's uuid (nonzero), its journal identity the uuid: the id
    travels with the index directory; a copied directory's second open is refused by the loop's table.
12. One journal host per data directory (`.journal`), two chore threads, a group on chore `id % 2`; closed
    records kept per process (a restarted process recovers with an election).
13. A write holds the turn across prepare, propose and the quorum answer: prepare reads the index as the previous
    write left it. One write per index in flight; T11/Q14 decide pipelining.
14. A9's rf > 1 cases are tested on the replica's application entry by entry (the deposed proposer's chunk
    placed in the new term): apply is a function of entry and state, so it is the sequence every voter applies.
15. A request's commit is the group's checkpoint (d4); the catalog's commit hook stays a bare strata commit (D169).
16. A stored blob's record in `app.vol-N` is its bytes, zstd-compressed at prepare only when smaller; the locator's
    type names the codec (`stored` / `zstd_compressed_stored`) and its size is the record's. Uploads are raw.
    Contract `xapiand:volume-file-set`.
17. A document bound for another shard than its upload's group is refused before anything is proposed
    (ClientError), not applied then reported as master did.
18. RESTORE's commands are proposed one after another, each awaited at quorum; D17's pipelining waits on T12.
19. The crash check's command passes `build/bin/xapiand`; blob_crash_check.sh accepts it as the build's binary.
20. The crash check (lead's command) configures without -G Ninja; the lead's to fix.
18. Removing the WAL removes its user-visible surface with it (the quest's outcome): the `:wal` HTTP verb, the
    `wal` info keys and metadata key, `--writers` and `--wal-writer-cache-size` (removed options, unknown arguments).
19. A pool with no journal (tests, the CLI) is durable only at its commits; no second log replaces the WAL there.
20. The mutant probes carry `--for` their criterion with `--fails-without`: the tool refuses `--fails-without`
    with neither `--for` nor `--task`, and the lead's rule forbids `--task` for probes.
21. A pool with no journal (tests only; the manager always sets one) holds no stored blob: a write naming one, an upload
    or a batch is refused with `invalid` before anything applies, and a stored read raises `invalid`. The data storage
    it used was the WAL era's second blob path; the group's file set is the one path (D16).
22. `harness/profile_bulk.sh` keeps its WAL layer: T11 profiles master's WAL build as the baseline the journal is
    measured against.
23. `harness/engine_crash_check.sh` defaults GRACE to 0: an acknowledged write is in the journal before its answer, so
    none may be missing after kill -9 (D2).
24. The `\x00` locator metadata read (`GET /index/:<locator>`) is removed with the data storage it read.
25. A group checkpoint leaves the engine as its own commit did: the Core owes nothing and holds the manifest's revision,
    set on the group's chore (read_applied after the checkpoint), never from the reactor while the chore may apply.
26. test_node's slow-commit case holds the group's chore instead of the endpoint's flush chore (the WAL-era commit's
    path, which no commit takes now); its claim is unchanged.
27. Data's invariant is that after construction and every flush each locator views the Data's own bytes, owned by
    Data::flush (it parses what it built); a pending Locator views its own holder or the caller's memory
    (`data(string_view)`), and a copy or move keeps that. Callers never reparse to work around a flush.
28. The codebase guide drops the storage library's write-cursor invariant and footer trap with the WAL's: no
    Storage<> volume is written in src now (the blob volumes are the group's file set).
29. DATA_STORAGE goes with the library (lead's instruction): it no longer selected a store, only whether stored blobs
    build; every index's group serves them, so there is one build shape. config.h stays included in pool.h and
    shard.h (comment dropped): about twenty files read XAPIAND_* macros through transitive includes, and a lost
    one fails silently.
30. database/utils.cc (outside the set) loses its storage.h include and a history comment: required for the build
    once the library is gone.
31. A4's test reads through test_pool's Shard (get_view, engine_for_read and engine::search), not HTTP: read-your-writes
    is the shard's refresh-before-read, the same under any handler; the mutant that serves the last refresh is the
    dirty mark the group's apply sets.
32. The `levels` task row's globs are widened to the quest's set where ack=none needs them: src/database/{shard,pool,
    handler}.*, flags.h, shard_group.*, src/engine/strata/shard.*, tests/test_node.cc (all in the quest's files).
33. ack=none answers 202 Accepted (RESTORE too): the write is placed, not applied, and its answer carries no _version
    or docid (contract xapiand:ack-read-levels).
34. A metadata write (a schema's, a setting's) is always proposed at quorum, whatever the request's level: a later
    write's prepare must read the schema it changed.
35. The turn's next holder waits for an ack=none write's apply (ShardEndpoint::unapplied_, take_turn): D3's "prepare
    from applied state" on a lone voter, so a prepare, a placement read or a commit sees it.
36. HTTP ?commit no longer checkpoints: it is ack=quorum&refresh=true (D4); COMMIT /idx/ is the checkpoint (Q8 (a)).
    RESTORE takes only the ack flag: its closing commit refreshes already.
37. Shutdown (lead msg on d17a82): the registry's control stays set until the control scheduler closes (Stop 2), so a
    detached task never runs synchronously on the thread that drains that loop; finish() leaves the control to its
    owner; DatabasePool::stop_names() stops the names' build up front, as set_control(nullptr) did.
38. test_node's Watchdog samples every thread on a hang (sample on macOS, gdb where installed): a diagnostic, not a fix.
39. The drain case's fix is the test's, not the code's: 015dd1385's abandon is right; the case now builds its
    precondition (an autocommit in a held checkpoint) from observed state, never a sleep.
40. The batch-pins case's watchdog is 120 s, named by its flush count; fewer flushes are the journal library's.
41. A journal that cannot be recovered ends the start with EX_IOERR and one critical line naming the directory, the
    library's error and a finite file-size limit; it is reported in the manager (where set_journal runs), not wrapped
    in ShardGroups.
