# x7-loop-stalls: what holds an HTTP loop past 1 ms, and where RESTORE's time goes

> Status: CURRENT (2026-11-12) profiles of master's code (Xapiand:49ebb4481's `src/`), the cause of each miss, the
> design of RESTORE's prepare as a chore, and x7-gate's rows at the fix (quest x7-loop-stalls A1-A3); the design of
> fix (c), RESTORE's writes off the loop, under review (quest x7-loop-fix-c A1).

Question: x7-gate (`docs/gates/phase7c.md` at Xapiand:df5eb9e06) misses T4 (every HTTP loop's p99 lag 24.6-28.7 ms
against the 1,024 us edge), T5 (loaded light p99 76.8 ms), T10's flood (p99 50.9 ms) and T12's RESTORE (0.288x pre-7c).
Which work holds a loop, for how long and how often, and where does RESTORE's time go against pre-7c?

## Method

- Binaries: x7-gate's build job 261111-171014-19268b, RelWithDebInfo, `-fno-omit-frame-pointer`, LTO off. Head is
  Xapiand:49ebb4481, whose `src/` equals master's (`git diff 49ebb4481 master -- src` is empty); pre-7c is
  Xapiand:142b2ca77. Copies at `$HOME/.scratch/x7-loop-stalls/<commit>/xapiand` (same shasum as the job's).
- Host: macOS 15.8.1, Intel Core i9-9980HK, 8 cores, 16 logical CPUs. Head runs 16 reactors, pre-7c 10 (each binary's default).
- Load: x7-gate's `harness/stall_load.py` at Xapiand:234fe089a (20,000 `accounts`, light open loop at 200/s, heavy
  closed loop at 4x the reactors: two aggregations, a wildcard, a phrase and a text search, plus one RESTORE stream into
  `restore-target`). RESTORE alone: x7-gate's `harness/profile_bulk.sh --mult 20`.
- Profiler: `sample(1)` at 1 ms. A thread's sample counts wall time in a non-idle stack (a leaf outside `kevent`,
  `__psynch_cvwait`, `os_sync_wait_on_address` and other waits), so it is wall on the thread, not CPU.
- Lag: the loops' own `reactor::LagProbe` (`xapiand_reactor_lag_seconds`), a 1 ms timer whose lateness is binned in
  log2 buckets with 4 sub-buckets; a percentile is its bucket's upper edge.
- Analyzer: a parser of `sample`'s call graph (per-thread busy counts, inclusive and leaf shares by frame).

## RESTORE: one reactor does all of it

Source: x7-gate's t12b job 261111-171048-c39d8d (exclusive, 3 ABBA pairs; `bulk/pb-<i>-<arm>.sample.txt` in its output).
Head restores 1,999-2,161 docs/s, pre-7c 7,039-7,866 (head/pre 0.284, range 0.274-0.284) while head spends 0.81x
pre-7c's CPU per document (0.609-0.664 against 0.751-0.818 ms, user plus system).

**Cause.** Head prepares and applies every object on the reactor that accepted the request, one after another
(`DocIndexer::index`, `src/database/handler.cc`: `co_await index_one(...)` per object, a checkpoint between objects).
That reactor thread is in a non-idle stack for 83-86% of the profile window in all three head arms (5,858 of 6,816,
5,569 of 6,731 and 5,568 of 6,666 samples), which is all of the restore: the window also holds node start and the
load's tail. Every other thread is mostly parked: the 16 `W` workers together have 1,457 busy samples (fsync and
`fcntl` of the chores), the WAL thread 1,123. So RESTORE runs at the speed of one thread, about 0.5 ms of loop wall per
document (10.2 s of the window's loop wall over 20,000 documents). Pre-7c spread the same work over 10 `DP` (prepare)
threads, 691-837 busy samples each, and 5 busy `DI` (index) threads, 667-1,293 each; its critical thread (`DI23`)
waited on the shard's writable checkout 40.5% of its timeline.

Where the loop's time goes (head, arm 2; arms 3 and 6 in parentheses), as shares of the loop's busy samples:

| Work on the loop | Share | Note |
|---|---:|---|
| `engine::strata::Shard::replace` (the shard's turn) | 45.1% (54.0, 52.8) | of which `finalise_write` 34.3% (44.2, 43.6) |
| `finalise_write`: decompress the record, set `_version`, recompress | 34.3% | `Data::get_obj` 6.2% (zstd decompress 4.2%), `Data::set_obj` 19.2% (zstd compress 16.2%), `MsgPack::serialise` 2.5% |
| `Schema::index` (fields, terms) | 29.3% (23.3, 23.2) | `GenerateTerms::text` 10.6% |
| strata apply (`apply_settling`, `basic_index::apply`) | 5.4% (4.0, 4.3) | |
| `DatabaseHandler::prepare` (outside `Schema::index`) | 5.2% (3.4, 2.9) | compresses the record once: 2.7% |
| zstd in total | 23.1% (28.8, 28.6) | 18.9% compress, 4.2% decompress |

`finalise_write` (`src/database/shard.cc`) is the same code in pre-7c: it reads the record that `prepare` already
compressed, changes `_version` (and `_id` for a new numeric id) and compresses it again. Pre-7c paid it on the `DI`
threads in parallel; head pays it on the one loop.

## HTTP loops under the T4/T5 mix

Source: E1a, job 261111-221539-18ce81 (exclusive): head's binary bare, the loaded phase for 20 s (29.2 s with the drain),
sampled for 12 s from 3 s in; `ps -M` per-thread CPU before and after the sample. It reproduces the miss: light query
p50 13.3 ms, p99 175 ms, p99.9 307 ms (3,419 queries, 200/s held, 0 errors); every HTTP loop's lag p50 2.56-3.07 ms,
p99 28.7-41.0 ms, p99.9 57-131 ms, max 69-143 ms (worst http15); the control loop's p99 5.1 ms. Heavy: 26,183 searches
(898/s) and 20,000 restored documents.

**The loops are not saturated; single turns are long.** The process used 186.9 CPU-seconds in the 29.2 s phase, 6.4 of
16 logical CPUs, and the 16 loop threads are 98% of it (7.0-8.0 CPU-seconds each over the 16.2 s sample window, 43-49%
of a CPU). The workers are parked: the 16 `W` and 8 `WL` threads have 831 busy samples together against the loops'
63,176. At under half a CPU per loop, a 1 ms timer waits for whatever turn is running when it comes due, so a p50 lag
of 2.6 ms means the typical turn holds the loop for milliseconds and the p99 of 29-41 ms means some turns hold it for
tens. Queueing is not the cause: the demoted queue runs one handle per pass (`task_asio.hh` `drain()`), so a timer waits
behind at most one demoted turn plus the ordinary posts ahead of it.

Shares below are of the loops' busy samples. The sampler stops the process to walk 51 stacks (4,358 samples in 12 s,
2.75 ms apart), which inflates busy wall (88-94% of samples against 43-49% CPU), so the profile gives where turns spend
their time, not how long they last; the durations come from CPU per request and from E2 below.

| Work on the loops | Share | Yields inside? |
|---|---:|---|
| Exhaustive scan with the aggregation spy (`exhaustive_search::scan` -> `AggregationMatchSpy::see`) | 48.4% | once per 4,096-ordinal window and between segments |
| of which `_terms` on an integer (`age`): `TermsAggregation::aggregate_integer`, 27.8% of all; `strings::format` of the key per match 5.0% | | |
| of which `_values` on a string (`contact.state`) | 10.7% | |
| Hit loop after the search (`search_view`, 100 hits for wildcard, phrase and text) | 24.2% plus `Shard::get` 19.4% | no |
| `Shard::get` -> `OwnedDocView`: the stored record and every value copied | 18.7% | no |
| of which `stored_plugin::reader::stored(ord)` decoding the hit's whole frame (a block of up to 64 KiB) into a new buffer | 14.4% | |
| of which that buffer's zero fill (`string::append(n, char)`) and copy; `madvise` from freeing it (82% of all `madvise`) | 8.7%, 4.2%; 5.2% | |
| `Locator::data` (zstd of Xapiand's record), `MsgPack` unpack and map teardown, JSON of the response | 5.2%, 2.3%, 3.3%, 3.2% | no |
| Top-k scoring of the wildcard, phrase and text searches | 1.5% | per window |
| HTTP input (`ConnectionInput::readable`), which runs a light request's first turn inline | 5.3% | |
| RESTORE (`Schema::index`, `Shard::replace`, `DocIndexer`) | 0.9% | per object |

Turn lengths, from CPU per request (the loops' 183 CPU-seconds of the phase times each share, over the phase's requests;
an estimate, since it assumes the sampled 12 s stand for the phase):

- **Hit loop: about 5 ms of CPU per heavy search, in one turn.** `search_view` reads its 100 hits one after another with no
  suspension point: `Shard::get` (`src/engine/strata/shard.cc`) `co_return`s at once, and the loop has no checkpoint;
  the JSON response follows in the same turn. 43.6% of 183 s over 15,700 wildcard, phrase and text searches is 5.1 ms,
  about 50 us per hit, of which the stored frame decode is about 17 us (a 64 KiB zero fill and copy, then a medium free
  that calls `madvise`).
- **Aggregation windows: about 8.5 ms of CPU per aggregation, in windows of 4,096 ordinals.** `exhaustive_search::scan`
  (`prospect/shards.hh`) awaits the slice once per window, and the host's visitor runs per match inside it. 48.4% of
  183 s over 10,483 aggregations of 10,000 (`gender: male`) and 20,000 (`*`) matches is about 0.56 us per match, so a
  full window holds the loop for about 2.3 ms whatever the 200 us `REQUEST_SLICE` budget says.

**One kind at a time (E2, job 261111-225130-793974, exclusive).** Each heavy kind alone at 16 search clients (the
RESTORE stream answered by the harness), 12 s loaded; RESTORE with one text client. These short queries saturate the
loops (128-146 CPU-seconds in 12.6 s), so a loop's lag here is its longest turn plus a little queueing:

| Kind alone | Searches/s | Lag p50, median loop | Lag p99, min / median / max loop | Kind's p50 / p99 |
|---|---:|---:|---:|---:|
| `text`, 100 hits | 1,712 | 0.77 ms | 12.3 / 14.3 / 14.3 ms | 7.6 / 46.0 ms |
| `wildcard`, 100 hits | 1,716 | 0.64 ms | 12.3 / 12.3 / 14.3 ms | 7.7 / 47.1 ms |
| `phrase`, up to 100 hits | 3,565 | 1.79 ms | 7.2 / 7.2 / 8.2 ms | 3.7 / 16.7 ms |
| `_terms` on `age`, all 20,000 | 886 | 1.79 ms | 6.1 / 6.1 / 7.2 ms | 15.0 / 65.4 ms |
| `_values` on `contact.state`, 10,000 | 2,108 | 1.28 ms | 3.1 / 3.1 / 3.1 ms | 6.5 / 44.0 ms |
| RESTORE plus one `text` client | 106 | 0.19 ms | 0.38 / 0.38 / 10.2 ms | 1,887 docs/s |

The 100-hit kinds hold their loops longest (p99 12-14 ms against 3-7 ms for the aggregations), as an unsliced hit loop
predicts; the aggregations stay over the edge by their windows. With RESTORE and one text client, 15 of 16 loops stay
under 0.4 ms and the worst (the restore's or the text client's; the probe does not say) reaches 10.2 ms. (Each search run shows 23 "other" errors: the harness's local RESTORE answer, a 599 every
0.5 s.)

**Cause.** Two kinds of turn outlast the 1 ms edge, and the slicing of phase7c.md D12 covers neither: (1) the hit loop
and response after a search, unsliced, about 5 ms per 100-hit search; (2) the exhaustive scan's 4,096-ordinal window,
whose budget check ignores the visitor's per-match cost, about 2.3 ms per full window for these aggregations. RESTORE is not on
the loops' critical path in this mix (0.9%).

## Candidate fixes, ranked

Loops (T4, T5, T10). Each bounds a turn without bringing back a pool (the quest's ruling):

1. **The hit loop yields** (built: Xapiand:00cd38f39, test `tests/test_loop_yield.cc`). `search_view` awaits the request's budget (`request.budget`, 200 us) between hits, as the
   search does between windows, so a turn ends within one hit (about 50 us) of the slice. `src/server/search_views.cc`
   (its only per-hit loop). Test: a search of N hits on a zero-length budget resumes once
   per hit; the mutant without the checkpoint resumes once.
2. **The scan's window counts the host's work** (prospect quest x7-scan-slices; this quest bumps the pin). With a visitor or a key, `exhaustive_search::scan` checks the slice
   every 256 ordinals instead of 4,096 (`prospect/shards.hh`, one constant and the window's end), so an aggregation's
   turn is about 0.15 ms at 0.56 us per match; without host callbacks the window stays 4,096 (its cost is in prospect's
   `sliced-search.md` numbers). A prospect commit, then the pin in `CMakeLists.txt`.
3. **Cheaper turns** (fewer slices, less CPU; not needed for the edge once 1 and 2 hold): read a hit's record through a
   frame buffer reused across the request's hits (strata's `stored(ord, stored_frame&)`), which drops the zero fill,
   the allocation and the `madvise` (about 13% of the loops' busy time); format a `_terms` integer key at render, not per
   match (5.0%).

RESTORE (T12), the lead's ruling of 11-11: (a) prepare (`Schema::index`, `finalise_write`'s record work) as an index
chore on the host `WorkPool`, the shard's turn keeping only the apply; one design review of phase7c.md's amendment
first; (b) compress the record once, at finalise (34-44% of the loop's RESTORE time is `finalise_write` decompressing
and recompressing what `prepare` compressed; built: Xapiand:2f8145379, `Data::set_obj(obj, compress)`); (c) apply per shard in parallel only if (a) and (b) leave T12 short.

## Design: RESTORE's prepare as an index chore

The lead's ruling (fix (a) above), revised for the design review's findings D1-D8
(`.campaign/xapiand/reviews/x7-loop-stalls-design.md`). It amends phase7c.md D4 (the `flush` row gains "RESTORE
prepare") and makes D12's "RESTORE batches are prepared in parse-ahead chores" true.

**What moves.** The CPU of preparing a plain object: its id and `_data`, all of `Schema::index` (fields, terms,
values, and the shard of a new uuid id, `seq % n_shards` checked with `Endpoint::is_active()`, an atomic read; a new
numeric id takes `NEW_NUMERIC_KEY` and the shard numbers it in its turn, D8), and the record serialised raw. The one
compress stays in `finalise_write`, in the shard's turn (fix (b)). It runs as `flush` chores on the one host
`WorkPool`; no thread or pool is added.

**Barriers (D1).** An object whose prepare reads the stored document or other loop-bound state takes today's path on
the loop, and only after every earlier object of the stream has applied: an `_op_type` other than `index` (`patch`,
`update` and `merge` read the current document), an object with a root `_script` (the script reads `_old_doc`) or
`_schema` (its schema is updated as it is fetched), and every object while the index's schema has a root script.
Plain `index` objects keep the parallel path. `DocIndexer::index` returns only after every object of its batch has
applied, so a barrier at a batch's start sees the previous batch; no prepare runs ahead of a batch boundary.

**Rounds and ordering (D2, D5).** The plain objects between two barriers form a run, prepared in rounds. A round
fetches the schema on the loop and posts one chore per non-reserve worker (`threads() - 1`). Each chore builds its
own `Schema` over the snapshot's locked `shared_ptr<const MsgPack>` (D4: `Schema::index` mutates its `Schema`, so
nothing else is shared) and pulls the run's objects in input order from one atomic cursor. It stops after about 1 ms
of work, at the run's end, or after its first object that changes the schema, so no chore holds a worker past about
1 ms plus one object (D5) and the parse's chores interleave. When every chore of the round has ended, the loop takes
the prepared objects in input order up to the first one that changed the schema and offers that change
(`SchemasLRU::set`): changes commit in input order. Every later object was prepared on the old snapshot and is
prepared again in the next round, against the new one (D2); completion order never shows, since the loop reads the
round only after all its chores end. A refused offer prepares the object again from a fresh snapshot, at most
`SCHEMA_RETRIES` times, as today. A change wastes at most the objects the other chores had pulled, about one per
worker, so a new index (whose first objects change the schema) costs about one round per change.

**Apply.** The loop applies the accepted objects in input order, one per shard turn (`replace_document_term`,
`finalise_write`), with a checkpoint between, while the next round's chores prepare the objects after them
(`task::when_all`, as the parse overlaps the batch). `_version` and new numeric ids are assigned in input order, as
today.

**Failure (D7).** Each object's prepare is caught inside the chore; its exception is that object's result, mapped on
the loop as `co_catch_http_errors` maps it today, and the others go on. A queued chore that `IndexChores::cancel_all`
drops resumes with `task::cancelled`; `task::when_all` rethrows only after every chore of the round has ended. The
request's token is read at each checkpoint between applies. What applied stays and the closing commit runs on every
exit (`restore_database_view`).

**Lifetime (D3).** A round's state (its objects, their prepared documents and errors, the cursor, the snapshot)
lives in the awaiting frame, as `Request::parse_ahead`'s does, and the chores reference it. A frame is never
destroyed under a running chore: `cancel_all` drains running chores before the loops' tasks are destroyed (`pool.h`),
and frames end by completing (D3 R6.3). A chore touches no loop-bound state: `Schema::index` reads the endpoints and
flags of a `DatabaseHandler` of the batch's own, never written while chores run. Prepared documents cross threads once,
written by a chore and read by the loop after the chore's completion resumes it (the pool's handoff orders them).

**Group.** `DocIndexer` takes the RESTORE's own `IndexChores` (`restore_database_view`'s, which its parse-ahead
chores already use), so parse and prepare share one group and one DRR share beside the index's group. The command
line's `--restore` has no reactor to free and passes `engine::inline_chores()`.

**Expected effect (D6).** From the RESTORE table's shares: with `Schema::index` off the loop and compress-once as
ruled, the loop keeps the shard's turn (`finalise_write` less its decompress, 34.3 - 4.2%, plus the rest of
`Shard::replace`, 45.1 - 34.3%), about 41% of today's loop time (about 50% in arms 3 and 6): 0.2-0.25 ms per object,
about 4,000-5,000 docs/s, 0.55-0.7x pre-7c. An estimate from profile shares; A3's job decides. If T12 stays short,
fix (c) (apply per shard in parallel) follows. The owner's question, not built: leave `_version` and a new numeric
`_id` out of the stored record and fill them at read from their value slots, which takes the compress off the turn
too (about 11% left). Its costs: `_dump_document` builds a dump from the record alone, so `dump_page` must return the
id and version slots or a restore renumbers numeric ids; `call_script`'s `_old_doc` and the bases of patch, update and
merge (`Data::get_obj`) need the same fill; tests of a dump-restore round trip of a numeric-id index and of `_version`
in GET, search and dump.

**Tests** (`tests/test_loop_yield.cc`, a node in the test's process; RESTORE over HTTP):

- An update after a run of plain objects in one batch (a JSON array), and an update that starts the second batch (an
  NDJSON body sent in two pieces with a pause): GET shows both fields. Mutants: the run before a barrier is not
  applied first; a batch's last run is not applied before `index` returns.
- Schema order: a new index restored with `x` an integer in an object slow to prepare, then `x` a string in a fast
  one, then an integer again; each object's result and the final schema equal a sequential restore's (one object per
  request), the string fails, and `x` is an integer in both schemas. Mutant: the round accepts the objects past its first change, prepared on the old
  snapshot.
- Prepare off the loop: the HTTP loop's CPU for a text-heavy RESTORE of 3,000 documents is under 45% of the
  process's. Mutant: every object is a barrier.

**Files.** `src/database/handler.{h,cc}` (`DocIndexer`, and the helpers it shares with `DatabaseHandler::prepare`:
`document_id_of`, `take_op_type`, `finish_prepared`, `offer_schema`), `src/server/search_views.cc` (the group),
`docs/design/phase7c.md` (D4's `flush` row, D12), `tests/test_loop_yield.cc`.

## Design: fix (c), RESTORE's writes off the loop, one shard per child (quest x7-loop-fix-c)

Not built; settled by one concurrency design review first (quest x7-loop-fix-c, A1). It builds on the prepare
chores above and keeps D1 (barriers) and D2 (schema changes in input order) as they are.

**Where RESTORE's time is at the fix.** From the A3 job's `profile_bulk.sh` samples (261112-012857-a77bf8, arms 3
and 6; arm 2's profile is invalid): the RESTORE's loop has 2,809 and 2,882 busy samples, the 16 workers 6,780 and
7,059. On the loop, `finalise_write` is 46.1% and 45.7% (zstd compress 29.4%), strata's apply (`apply_settling`)
9.3% and 8.6%; the two are disjoint, so the write in the shard's turn is at least 54-55% of the loop's time. On the
workers, `Schema::index` is 67.7%. The loop runs every object's write one after another, so RESTORE gets about
2.6 busy cores where pre-7c gets 5.9, at 0.76x pre-7c's CPU per document (the A3 table below).

**What moves.** Each object's write: `ShardEndpoint::do_replace_term` (or `do_replace` for a document that exists),
that is the blob push, `finalise_write`'s record (`_version`, a new numeric `_id`, the one compress) and strata's
`replace_by_key` with its apply. It runs in a `flush` chore of the RESTORE's group on the one host `WorkPool`, while
the task that asked for it holds the shard's turn, so "every apply stays in the shard's turn" (D12) still holds and
no thread or pool is added. The loop keeps placement, results, counters and the round's sequencing.

**The chore.** It is `commit_blocking`'s shape (`src/database/shard.cc`), which already runs engine writes on a
worker: the engine's chores are swapped to `engine::inline_chores()` for its run and restored on every exit,
blocking is allowed (`engine::BlockingAllowed`), and each write runs under `engine::sync_wait`; an owed flush or
drain then runs inline on that worker. The chore passes `engine::inline_chores()` to `do_replace*` too, as
`commit_blocking` does to `do_commit`, so a blob push runs on the same worker instead of waiting on another chore of
the pool. One RAII guard serves both. The swap is sound only while the swapper holds the turn and every user of the
engine's chores holds it too: writes, `fresh()` (`pool.cc:362-371`), `written_key`, commit and check take it, and
`reclaim` and the closes have no caller in `src/` (the design review's Q1). The chore applies its shard's objects in input order until about 1 ms has passed (the prepare's `PREPARE_SLICE`,
D5), catching each object's failure as that object's result (`WriteResult` or `exception_ptr`, filled by the chore
and read by the loop after the chore's completion resumes it).

**Children, one shard each.** After a round's join, the loop places the accepted range `[at, stop)` (below) and
splits it into one sub-run per shard, in input order. Each sub-run is a child task, and `apply_run` awaits them with
the next round's `pull_round` in one `task::when_all`, where it awaits `apply_range` today. A child repeats: pin the
shard, take its turn, await one apply chore, release the turn; a sub-run's shards are served by at most `W`
children, `W = min(sub-runs, max(1, (threads() - 1) / 2))`, each taking the next unserved sub-run when its own is
done, so at most `W` turns are held at once and each held turn has a worker to run its chore (the review's N1). The
rule "a task holds one index's turn at most"
(`pool.cc:354`, counted per request in debug builds) is kept by running each child under `engine::counted`
(`host_asio.h`), so each child is a task with its own counters and holds one turn; the parent holds none while it
awaits them. No child waits for a second turn while it holds one, so the children cannot deadlock each other or
other writers. A turn is held for one chore: its wait in the pool's queue (other groups' work, by DRR) plus about
1 ms, instead of one object today; A2's same-index run reports it. The prepare chores take `threads() - 1 - W`
workers (at least 1), so a round's prepares and the applies fit the non-reserve workers. With no reactor (the CLI's
`--restore`, inline chores) every object takes today's path, `apply` through `replace_document_term` in input order,
with no apply chore and no nested `engine::sync_wait` (N7).

**Placement.** Today's `replace_document_term` places each object on the loop in its turn; the parallel apply needs
the placement of the whole range first. After the join, the loop asks each shard once, in its turn, which of the
range's terms it holds (`written_key` for each, one turn per shard per range instead of one per shard per object).
It keeps `find_docid_term`'s rules: shards in order, the first that holds a term wins, a shard's read retries and
reopens as there, and a shard that is `not_available` fails only the objects no other shard holds (N3). A term found in a shard goes to that shard as `do_replace(did, must_exist)`; a new term goes to `numeric_docid` or
`key_placement_hash`, as today. A term's objects in one range all reach one shard: the lookup and the hash answer the
same for each, since nothing in the range erases (an erase is a barrier). Two cases go back to today's serial path
after a join, applied alone: a new numeric id (`NEW_NUMERIC_KEY`), since `least_used_shard` reads the counts the
range's earlier writes change, and a `must_exist` write whose document left its shard meanwhile (a concurrent erase
between placement and apply). At such a miss the chore defers that object and every later object of its term in the
sub-run (a term's objects all share one sub-run), and after the join the deferred objects take today's path in input
order, placed afresh: a concurrent write that reuses the id between the two cannot be overwritten by an earlier
object of the stream (the review's B1). A dump carries its ids, so neither case is on a dump restore's path.

**Ordering.** Per id: all its writes reach one shard and apply in input order there, so `_version` (assigned by
strata in the turn) and the record equal a serial restore's. Across ids on different shards there is no order to
keep: only a reader of the stream's own writes could see one, and those are barriers (D1), which join every child
first. New numeric ids are numbered serially (above). Schema changes: `accept` offers them after the join, as today
(D2); every object of the range was prepared on the schema the range applies under. A batch's end joins too.

**Failure and cancel.** An object's write failure is its result, as today; the other objects and shards go on. A
child whose pin or turn fails (busy, closed, cancelled) fails its sub-run's remaining objects with that error, each
its own result. The children take the turn with the request's token, so a client gone stops each child at its next
slice; `when_all` lets every child end, what applied stays, and the closing commit runs on every exit
(`restore_database_view`). A wider failure is the owner's call (the review's N4): a child whose pin or turn fails
fails its sub-run's remaining objects where the serial path gives each object its own turn wait, and inside a slice
an error that marks the endpoint stale leaves the slice's later objects to the stale index, where the serial path
reopens at release. Options: (1) keep that; (2) end the slice at a stale-marking error, and on busy fail the slice's
first object and retry the rest in a new turn. The build does (2) for the stale case only if the owner rules so;
otherwise each object's failure stays its own and a turn failure fails the sub-run's rest. Lifetimes: a sub-run's objects and results live in the awaiting frame, as the round's do
(D3); a child's chore ends before the child releases its turn.

**What each row should gain.**

- T12 (RESTORE docs/s >= 1.0x pre-7c; 0.571x and 0.594x at the fix). The whole turn (`Shard::replace*`) is 69-71%
  of the loop's RESTORE time (the review's recount), so the loop keeps about 30% and its ceiling rises about 3x. The
  critical path after the fix is the slowest of: the per-shard writes, about 0.1 ms per document over `W` children;
  the prepares, about 0.34 ms of CPU per document over `threads() - 1 - W` workers; and the round barrier (`when_all`
  of a range's apply and the next prepare). CPU per document stays about 0.6 ms (0.76x pre-7c's), plus a chore hop per
  shard per slice. If it keeps pre-7c's 5.9 busy cores (assumed, not derived), that is about 9,800 docs/s, about 1.3x
  pre-7c; 1.0x needs 4.5 busy cores. A3's job decides.
- T12 saturated QPS (2.69x): unchanged; no read path changes.
- T4 and T5: nothing measurable. RESTORE is 1.3% of the loops' busy time in the mix (E3), and fix (c) takes about
  half of that off. Risks to watch in A2: the apply chores occupy up to 5 more workers during the mix's RESTORE (the
  query class keeps its reserve worker, so a light read-your-writes refresh is not queued behind them); the
  same-index run (RESTORE into `accounts`, reported) holds an `accounts` shard's turn for one chore, about 1 ms, so a
  light write or refresh on that shard can wait that long.

**Tests** (`tests/test_loop_yield.cc`; the RESTORE tests write indexes of 5 shards on a node of one HTTP loop, whose
WorkPool has a thread per CPU; each new handoff has a mutant that fails it, run as a job). A wall-time range can hold one object on a
Debug or TSan build, so a mutant could pass vacuously (the review's B2): a test seam sets the objects each prepare
chore takes (a count instead of `PREPARE_SLICE`), and `DocIndexer` reports what the run met (the largest range, ids
repeated within one sub-run, the peak number of children applying, the objects applied serially after a join, the
deferred objects). Each test asserts its case occurred before it checks the outcome.

- Model equality: one RESTORE of a stream with ids repeated across it (a later object replacing an earlier one), ids
  on every shard and ids that already exist, against the same stream restored one object per request into another
  index: every result (status, `_id`, `_version`) and every GET equal; the run met a range of at least 16 objects
  and a repeated id within one sub-run. Mutant: each shard's sub-run applied in reverse order.
- Writes off the loop: the HTTP loop's CPU for a 3,000-document RESTORE is under a threshold set halfway between the
  build's share and the mutant's (34% at the fix, the prepare test's measure). Mutant: the writes run on the loop.
- Shards in parallel: `DocIndexer` reports the children cap and the peak number of shards applying at once; with 5
  shards and a cap above 1 (a pool of 5 threads or more) the peak exceeds 1. Mutant: the children are awaited one at
  a time.
- New numeric ids: an id-less RESTORE into a numeric-id index of 5 shards gives the same ids, shard by shard, as the
  one-object-per-request run; the run met a range holding more than one such object. Mutant: no join before a
  `NEW_NUMERIC_KEY` object.
- A miss between placement and apply (B1): a test seam runs on the loop after placement and before the children, and
  erases id 7 and writes it again through the RESTORE's handler; the stream holds three objects of id 7 in one range.
  The final document is the stream's last object of id 7, and the run reports all three deferred. Mutant: a missed
  write fails as today's `must_exist` miss does. (A later write of the same id carries the same placement, so it
  misses too; a mutant deferring only the first miss is not observable.)
- The chores swap (N5): a test seam makes one apply slice throw; afterwards no engine runs its chores inline
  (`ShardEndpoint::engines_inline()`, the guard's count), and a later RESTORE's write runs. Mutant: the guard
  does not restore on the throw path.
- D1 and D2 as today (the update after a run, the schema order test), unchanged and passing.
- Races: the reviewer's `restore_model.py` and the RESTORE tests on a TSan build, a light job (D61).

**T4 and T5: what they need, beyond fix (c).** At the fix every idle HTTP loop shows lag p50 320 us and p99 512 us
(the timer's own wake-up on this host); loaded, p50 is 448 us but p99 is 3.0-5.1 ms and p99.9 6-12 ms (A2 job
261112-012855-b34e3c). A queue of 200-300 us turns at the loops' 42% CPU gives a p99 near 1 ms by an M/D/1 estimate, not 3 ms, so the
tail needs events of several milliseconds: either single turns that long (H1: the per-hit 64 KiB frame buffer's
free, `madvise` at 13.3% of loop time, taking a page-release path) or a loop thread descheduled (H3: 16 loops, 16
workers and the load generator on 16 logical CPUs). Tuning `REQUEST_SLICE` (D2's estimate, in this file set) addresses
neither. E4, one exclusive job of at most 10 minutes: x7-gate's loaded mix on an experiment build (not landed) whose
Asio custom handler tracking records each loop's handler durations in log2 bins with the handler's operation; turns of
2 ms or more covering about 1% of the window confirm H1, and fix 3's frame buffer and `_terms` key then follow
(`src/engine/strata/shard.cc`, `doc_view.{h,cc}`, `src/aggregations/bucket.h`, outside this quest's file set);
otherwise H3 is next, measured with the generator's CPU held apart.

**E4: long turns are mostly a descheduled loop (H3).** Job 261112-042106-305a1a, x7-gate's loaded mix on the
handler-tracking build (Xapiand:4bcb4494c, 16 CPUs, a 23.7 s window; the tracking costs about 19% of heavy searches,
21,211 against 26,066). Per HTTP loop, median of 16 (max):

| Per loop | Median (max) |
|---|---:|
| Busy share of the window | 56% (71%) |
| CPU per wall second while in a handler | 0.85 (0.86) |
| Handlers of 1 ms or more per second, and their CPU per wall second | 35.8 (47.9), 0.45 (0.56) |
| Share of the window a tick waits 2 ms or more: in one handler, in one busy period | 4.1% (5.5%), 8.4% (25%) |
| Busy periods of 1 ms or more per second, their mean length, the mean longest handler in one | 145 (181), 2.1 ms (3.0), 1.02 ms (1.08) |
| Probe lag p99, p99.9 (plain build: p99 4.1-6.1 ms) | 6-8 ms, 20-57 ms |

Turns of 2 ms or more cover 4% of the window, so H1's threshold is met on wall time, but those turns run at 0.45 CPU
per wall second: about 1 ms of work each, stretched by time off the CPU. 13,152 of the 13,857 handlers of 1 ms or more
are `io_context/execute`, the coroutine resumptions (36 s of wall at 0.47 CPU per wall second). A busy period
is a queue of such 1 ms turns. The host runs 16 loops, a WorkPool thread per CPU and the load generator on 16 logical
CPUs, so the scheduler takes a loop off its CPU mid-turn. Fix 3 shortens the turns' CPU (the frame buffer alone is
up to 37% of loop time, E3); T4 and T5 also need fewer runnable threads than CPUs or loops scheduled ahead of the
workers (thread counts, or a higher QoS class for loop threads on macOS), outside this quest's file set.

**Steps.** (1) This design and its review. (2) The apply chore, placement and children; the tests above; the suite
and the mutants as jobs. (3) A3: T12 by x7-gate's procedure, one exclusive job. (4) A2 with fix (c), and E4, each one
exclusive job. phase7c.md's D4 `flush` row gains "RESTORE writes" and D12's RESTORE sentence names the write chore;
that file is outside this quest's file set.

## At the fix: x7-gate's rows (A2, A3)

Binaries: the fix, Xapiand:0072ba93e (fix 1, compress-once, RESTORE prepare as chores, prospect b20e7c5's 256-ordinal
scan window; strata 5d742b24), job 261112-012852-295929, shasum 5e9c9693; pre-7c, Xapiand:142b2ca77, the same bytes
as x7-gate's build-pre (shasum 787b7174). RelWithDebInfo, frame pointers, LTO off. Host as above (16 logical CPUs).
x7-gate's procedure (D59), exclusive: A2 is job 261112-012855-b34e3c (ABBA pre, cur, cur, pre; then the same-index
run on cur), A3 is job 261112-012857-a77bf8 (T10 on cur; T12 ABBA pre, cur, cur, pre, pre, cur for `loadtest.py` and
for `profile_bulk.sh`). Ratios are cur/pre per adjacent pair. "Gate" is x7-gate's number at Xapiand:df5eb9e06.

| Row | Target | Pre-7c | Fix | Gate | Verdict |
|---|---|---:|---:|---:|---|
| T4 every HTTP loop's p99 lag, loaded | <= 1,024 us | 640, 768 us (all 10 loops) | 2.56-5.12 ms (all 16 loops over; median loop 3.58, 2.56 ms) | 24.6-28.7 ms | MISS, 2.5-5x the edge (worst loop about 5x lower than the gate) |
| T5 loaded light p99 | <= 2x unloaded p99 + 1 ms, and below pre-7c | 414.6, 355.0 ms | 27.1 ms (bound 10.2), 22.7 ms (bound 8.9) | 76.8 ms (bound 8.93) | MISS on the bound by 2.5-2.7x; below pre-7c (0.065x) |
| T5 same-index run (RESTORE into `accounts`), reported | | | light p99 39.0 ms, worst loop p99 8.19 ms | 98.2 ms | reported |
| T4 beside 4 looping scripts | <= 1,024 us | | worst loop p99 5.12 ms (http2), light p99 19.7 ms | | MISS, as T4 |
| T10 16-client flood, light-GET p99 | about 20 ms (the per-call cap plus the host's own) | | 45.4 ms (max 50.2; p50 0.37) | 50.9 ms | MISS, 2.3x |
| T12 RESTORE docs/s, `loadtest.py` | >= 1.0x pre-7c | 7,629, 7,375, 7,468 | 3,989, 4,209, 4,461 | 0.288x | MISS, 0.571x (0.523-0.597) |
| T12 RESTORE docs/s, `profile_bulk.sh` | >= 1.0x pre-7c | 7,382, 7,824, 7,490 | 4,449, 4,649, 4,356 | 0.284x | MISS, 0.594x (0.582-0.603) |
| T12 saturated QPS, 16 clients | >= 1.0x pre-7c | 1,651, 1,463, 1,447 | 4,435, 3,937, 4,064 | 2.6x | PASS, 2.69x (2.69-2.81); query p99 0.33x |

Every other number in the rows held: no errors in any arm; unloaded light p99 4.6 and 4.0 ms against pre-7c's 31.9 and
30.6 ms; T10's other light-GET cases stay under 1 ms at p99 (0.70 ms beside 4 long scripts and beside 4 library-call
loops). `profile_bulk.sh` exits 3 in arm 2: its sampled profile is invalid (839 samples), its smoke number stands.

**What moved.** The loops' worst turns are gone: under the T4/T5 mix the median loop's lag p99.9 fell from 57 ms to
6-7 ms and its maximum from 69 ms to 20-22 ms, while the fix answers 5.6-6.0x pre-7c's heavy searches (1,332-1,364/s
against 221-245/s; the gate's head did 1,199/s). Light queries under load are 15x faster than pre-7c at p99. RESTORE
doubled (0.29x to 0.57-0.59x pre-7c), at the D6 estimate's low end (0.55-0.7x).

**What still misses, and why (from these jobs).**

- T12: RESTORE is short of cores, not of CPU. The fix spends 0.76x pre-7c's CPU per document (0.583-0.610 against
  0.767-0.796 ms, user plus system), but keeps 2.6-2.7 cores busy (11.7-12.2 CPU-seconds in 4.3-4.6 s) where pre-7c
  keeps 5.8-6.1 (15.4-15.9 in 2.6-2.7 s). The prepare runs on the pool; the apply stays serial in one shard's turn and
  the rounds wait for it. At pre-7c's occupancy the fix's CPU per document would give about 1.3x pre-7c. Next: fix (c),
  the apply per shard in parallel (the lead's ruling).
- T4 and T5: no single turn of tens of milliseconds remains, but every HTTP loop still sees 2.5-5 ms at p99. Profile
  E3 (job 261112-020117-4c8576: E1a's procedure on the fix's binary, 16.7 s sampled) shows what the loops run now.
  They are not saturated: each loop used 6.9-7.2 CPU-seconds in the window (42% of a CPU), 7.1 cores in all. Shares of
  the loops' 46,222 busy samples:

  | Work on the loops | Share | Bounded by |
  |---|---:|---|
  | Exhaustive scan with the aggregation spy | 37.7% | 256 ordinals with host work (prospect b20e7c5) |
  | of which the spy per match (`TermsHandler`/`ValuesHandler::values`, `sortable_unserialise`, a key string per match: malloc, free, memcmp) | 32.9% | |
  | Hit loop (`search_view`) | 23.8% | one hit (fix 1) |
  | `Shard::get` -> `OwnedDocView`, 97% of it from the hit loop | 29.4% | one hit |
  | of which `stored_plugin::reader::stored`, the hit's whole frame decoded into a new buffer | 23.4% | |
  | `madvise` from freeing that buffer | 13.3% | |
  | Record decompress (`Locator::data`), JSON writer, aggregation results | 4.6%, 3.0%, 1.7% | |
  | RESTORE on the loop (`Shard::replace`, `finalise_write`, inline prepares) | 1.3% | one object |

  Every turn now has a bound of one hit or 256 ordinals plus the 200 us slice, so the 2.5-5 ms p99 is a timer waiting
  behind several such turns of the loop's 4 heavy requests, and behind the slow ones among them: a hit whose frame
  decode and free (`madvise` is 13% of the loops' time in one syscall per hit) takes a page-release path. The
  sampler cannot time single turns; a per-turn duration histogram on the loop (beside the lag probe) would. The two
  largest costs are fix 3's: a frame buffer reused across a request's hits removes the decode's allocation, zero fill
  and `madvise` (up to 37% of loop time, the two shares may overlap), and a `_terms` key formatted at render, not per match, removes
  most of the spy's allocation. T5's bound follows T4: a light query waits behind the same turns.
- T10: unchanged by this quest (50.9 to 45.4 ms). A worst-case script call is one non-yieldable library call up to the
  per-call cap (about 20 ms), and with 16 flood clients on 16 loops a light GET can land behind one running call and
  one queued (p99 45 ms, max 50 ms, about two caps). That bound is the Lua quest's (`src/lua/**`), outside this quest's
  file set.

## Reproduce

The jobs' commands are in their specs (`campaign job show <id>`); each runs at Xapiand:234fe089a for x7-gate's harness,
against the binary copies above.

- E1a, the sampled mix: `python3 harness/stall_load.py --bin <head> --accounts-docs 20000 --phase loaded --rate 200
  --duration 20 --no-assert --out A-mixed.json`, with `sample <pid> 12 1` from 3 s into the loaded phase and `ps -M -p <pid>`
  before and after.
- E2, one kind at a time: the same with `HEAVY_QUERIES` narrowed to one kind and the RESTORE stream answered locally
  (a wrapper that patches `stall_load`'s module), `--heavy-clients 17 --duration 12`; RESTORE with one text client,
  `--heavy-clients 2`.
- Shares: a parser of `sample`'s call graph (per-thread busy counts; a frame's share is the busy leaf samples whose stack
  holds it; idle leaves are the waits named under Method).
