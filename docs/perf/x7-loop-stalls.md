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
and recompressing what `prepare` compressed); (c) apply per shard in parallel only if (a) and (b) leave T12 short.

## Design: RESTORE's prepare as an index chore

The lead's ruling (fix (a) above); for one design review before it is built. It amends phase7c.md D4 and D12, whose
D12 already says "RESTORE batches are prepared in parse-ahead chores" while master prepares them on the loop.

**What moves.** The CPU of preparing an object, per sub-batch of a parsed batch: walking the object against a schema
snapshot, generating fields, terms and values into an `engine::Document`, and serialising the record (where it is
compressed is the open point below). It runs as a `flush` chore in the RESTORE's own `WorkGroup` (the group `restore_database_view`
already makes for its parse-ahead chores), so the one host `WorkPool` serves it beside the parse and no thread or pool
is added. A batch splits into up to `workers` sub-batches posted together; each returns its prepared objects in input
order.

**What stays on the loop.** Everything that awaits or touches loop-bound state: the schema lookup and its update
(`get_schema`, `update_schema`, control-plane caches); an object's id and the current document for `patch`, `update`
and `merge` (reads through the shard's view); shard placement for new numeric ids (it pins shards for their counts);
and the apply in the shard's turn (`replace_document_term`, `finalise_write`). This needs `Schema::index` split in
two: a pure part over `(const Schema&, object, Data)` with no `DatabaseHandler`, and the loop part (placement, scripts
that read the old document) around it.

**Ordering.** Objects apply in input order, as today: the loop task awaits a batch's chores, then applies its objects
one by one in its turns, with a checkpoint between objects. The next batch's chores may run while this batch applies
(one batch ahead, as the parse already is), so prepare overlaps the apply. A schema change found while preparing (a
new field) stops that sub-batch at the object: the chore returns the objects before it and the changed schema; the loop
runs `update_schema` and posts the rest against the new snapshot. Two sub-batches that change the schema at once
apply their updates one at a time on the loop, and the second re-prepares from its first changed object (the existing
`SCHEMA_RETRIES` rule, moved to the batch).

**Failure.** An object's error (bad field, type mismatch) is that object's result, as `co_catch_http_errors` makes it
today; the others go on. A chore that throws (allocation, a bug) fails its sub-batch's objects with 500 and the
request goes on. Cancellation: the request's token stops posting chores and is read between sub-batches; a cancelled
RESTORE awaits its group's in-flight chores (its own group, so no chore waits on its own group) and drops what they
prepared. Shutdown cancels the group through `IndexChores::cancel_all`, as for the parse.

**Lifetime across threads.** A sub-batch's inputs and outputs live in one `shared_ptr` state: the objects moved in, the
schema snapshot (`shared_ptr<const Schema>`, immutable), and the prepared `engine::Document`s, records and errors. The
chore holds one reference and the loop task the other, so a task destroyed while suspended (teardown phase 3) leaves the
chore its state until the chore ends; the chore resumes the task with `post()` only if it still waits. Nothing in the
state points into the task's frame, the `DatabaseHandler` or a view. Built documents cross threads once, chore to loop,
by that `shared_ptr` (release on the worker, acquire on the loop through the post).

**Expected effect, and the open point.** `Schema::index` plus `finalise_write` are 64-68% of the loop's RESTORE time
(the RESTORE section: 29.3 + 34.3, 23.3 + 44.2, 23.2 + 43.6). The chore takes `Schema::index` off the loop. Fix (b) as
ruled, compressing once at finalise, removes the decompress and the second compress but keeps one zstd compress in the
shard's turn (16-19% of that time is compression). The alternative for the review: leave `_version`, and the `_id` of a
new numeric id, out of the stored record and fill them at read from their value slots (`search_view` and
`retrieve_document_view` already fill both when a record lacks them); then the record is compressed once in the chore
and `finalise_write` does no record work. It changes what the record holds (formats are open until release); a
response's field order may change, and `Document::get_obj`'s callers (patch, update, scripts) need the same fill. Fix (c), apply
per shard in parallel, only if T12 stays short.

**Files.** `docs/design/phase7c.md` (D4's `flush` row gains "RESTORE prepare", D12's sentence becomes true),
`src/database/schema.{h,cc}` (the split), `src/database/handler.{h,cc}` (`DocIndexer`, `prepare_document`),
`src/server/search_views.cc` (`restore_database_view`'s batch loop), `tests/test_reactor_requests.cc` (prepare off the
loop: loop CPU per batch against inline, as the bulk-parse test measures; cancellation and teardown with chores in
flight).

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
