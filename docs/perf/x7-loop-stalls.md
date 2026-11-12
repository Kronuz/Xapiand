# x7-loop-stalls: what holds an HTTP loop past 1 ms, and where RESTORE's time goes

> Status: CURRENT (2026-11-11) profiles of master's code (Xapiand:49ebb4481's `src/`), the cause of each miss, and the
> design of RESTORE's prepare as a chore. Quest x7-loop-stalls A1.

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
  request), and the string fails. Mutant: the round accepts the objects past its first change, prepared on the old
  snapshot.
- Prepare off the loop: the HTTP loop's CPU for a text-heavy RESTORE of 3,000 documents is under 60% of the
  process's. Mutant: every object is a barrier.

**Files.** `src/database/handler.{h,cc}` (`DocIndexer`, and the helpers it shares with `DatabaseHandler::prepare`:
`document_id_of`, `take_op_type`, `finish_prepared`, `offer_schema`), `src/server/search_views.cc` (the group),
`docs/design/phase7c.md` (D4's `flush` row, D12), `tests/test_loop_yield.cc`.

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
