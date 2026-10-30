> Status: CURRENT (2026-10-30) code review of phase 7c step X2b (reader views; handler, schema, resolver and views as tasks), branch `phase7c/x2b-handlers-views` at `Xapiand:ac90387a5`.

# Phase 7c X2b code review

- Reviewed: `git diff db50b90b3..ac90387a5` (6863b1076 reader views, 690d2cfc3 tasks, tests, docs, ledger). Design: `docs/design/phase7c.md` D5 to D7, `docs/reviews/phase7c-design.md` C9, F16. Ledger: `docs/status/phase7c-x2b.md`.
- Built/run: the developer's Release server `.scratch/build-rel/bin/xapiand` (newer than the last `src` commit), macOS, `quiet check` clean. No sanitizer build (the developer's were removed; TSan was not rerun, see Not run).
- Probes (all kept in `.scratch/review-x2b/`): `ryw.py`, `ryw_bg.py`, `bulk_bg.py`, `churn.py`, `fanout.py`, `fanout2.py`, `probe_timeout.cc` (a copy of `tests/test_engine_host.cc` helpers plus one test, linked with the test target's flags), `scan_cost.cc`; results in `ryw-results.txt`, `churn-results.txt`, `fanout-results.txt`, `scan-cost-results.txt`. All servers I started are stopped. (Ports 18772 to 18775 are Detent's qemu forwards; I moved to 28771+ and did not touch those VMs.)

## Verdict: APPROVE WITH CONDITIONS

Risks 1 to 3 hold: no read-your-writes miss in about 25,000 probe iterations, the close protocol drains and times out as written, and the atomics cannot skip an owed commit. Risk 4 fails: the registry bound makes a valid multi-index search impossible, not only overloaded (X2b-B1). One blocker; it is a sizing and error-class fix, not a redesign.

## Blockers

### X2b-B1. A single search over more shards than the registry holds gets 503 on an idle server, and sizing ignores multi-index fan-out

- `src/manager.cc:235`: `num_http_servers * 1000 * num_shards`. `src/engine/strata/search.cc:652-656` takes one view per distinct index in the request, all held for the whole `run`. Records needed = indexes (shards) in the request, whatever else is running.
- Probe `fanout.py` + `fanout2.py` (`--http-servers 1 --shards 1`, capacity 1,000; 1,100 one-document indexes), idle server, no other traffic:

  | search over (comma list) | result |
  |---|---|
  | 900, 990 indexes | 200, all hits |
  | 1,010 and 1,100 indexes | **503** "Too many searches are reading at once: strata: every hazard record is claimed", every time |

  Wildcards and comma lists are documented (`docs/src/content/docs/reference-guide/api.md:264`). The same search worked before X2b (one Pinned core at a time). The `1000` is a literal with no stated mechanism: D2's per-reactor admission cap (the "in-flight cap" the formula multiplies) is not built, and the shards-per-request factor is the default shard count of one index, not the fan-out of a request. So the limit is not load-bearing against what it claims to bound, and the 503 text is wrong for this case (it is not "at once"; retrying never helps, and a client or LB will retry a 503).
- Not the design's C9 closure: `phase7c-design.md` re-check 1 already says "the sizing is not an upper bound: a multi-index search holds a guard per index and shard, so the 503 path is what bounds it". That is true only if the 503 is a transient overload. Here it is a permanent refusal of a valid request. At the many-indexes target (millions of indexes, wildcard queries) it is the common case.
- Required before landing (one of):
  1. A named constant for the per-request fan-out (`max_indexes_per_search`), registry = reactors x in-flight x that, and a request over the fan-out answered 4xx (`errc::invalid`, with the limit in the message), distinct from the 503; the formula and the mechanism in `reader-views.md` Limits. Or
  2. Views taken in waves for searches whose snapshot allows it (not possible with the single global snapshot in `run`; so this is the design question below).
  Either way a test: a search over capacity+1 indexes on an idle pool does not answer the overload 503. Mutant: drop the check, the test fails.
- Design question for the architect (not a reason to rewrite): a global snapshot over N indexes needs N simultaneous records. Is that the intended model at millions of indexes, or does a wide search need a bounded-fan-out rule (D6/D7)?

## Non-blocking

### X2b-N1. Registry memory and scan cost are not stated (T13 asks for the number)

Record = one cache line (`hazard_line`: 64 B here, 128 B on arm64 Apple per `strata hazard.hh:61-63`). Defaults on this 16-CPU host: 10 servers (MAX_HTTP_SERVERS) x 1000 x 5 shards = 50,000 records = 3.2 MB (6.4 MB at 128 B); a 64-CPU host with the cap lifted would be 320,000 = 20 MB. `scan()` walks all records on every publish (`hazard.hh:143-155`, twice when it takes the stamp lock). Probe `scan_cost.cc` (idle, x86_64 macOS, -O2): 128 records 0.2 us; 1,000 1.5 us; 10,000 26.6 us; **50,000 135 us; 320,000 2.8 ms** per scan, charged to every refresh/commit/free_views of every index. At 135 us per refresh of an index the scan can exceed the refresh itself for small indexes under the many-indexes workload. Put the two numbers in `reader-views.md` and in T13; if the registry is made smaller by B1 this shrinks with it. I did not measure the scan inside a real refresh.

### X2b-N2. A close or reopen that outlives the 30 s drain is dropped, not retried

`Shard::close` throws `busy` after `drain_timeout` (`shard.cc:196, 503`); `close_marked` logs it and sets the entry `empty`, dropping the engine while the reader still holds a view (`pool.cc:550-567`). Probe `probe_timeout.cc`: close with a held reader answers `busy` after exactly 30.0 s; with the shard dropped before the reader lets go and the reader released later, the same path opens again and reads K1 (no crash, no lock left). This is safe as far as I could probe (no ASan run), and only reachable by a reader that outlives its entry's reference, which `engine::search` does not (the views live on the stack of the synchronous `run`; no `Viewed` crosses a `co_await` in `shard.cc`/`search.cc`). Note for X3: when searches become tasks that await while holding a view, the 30 s wait while holding the index's turn becomes a deadlock-until-timeout against any awaited `fresh()` of the same index. State that rule in `reader-views.md`.

### X2b-N3. `Core::note` is a snapshot; readers of it tolerate staleness, and I found no skipped commit

`owed`, `committed_revision`, `committed_length` are relaxed atomics written only by the turn holder at the end of refresh, commit, reclaim, open and the flush chore (`shard.cc:253-266`, callers at 306, 328, 760, 1330). Reasoning checked:

1. The autocommit decision uses `ShardEndpoint::modified`/`applied_`, not these; `do_commit` reads `eng.revision()` before and after its own commit under the turn (`database/shard.cc:684-690`), so it reads its own store. No skipped commit.
2. `housekeeping()` pre-checks `merges_pending() == 0` outside the turn (`database/shard.cc:585`): a stale zero defers a reclaim by one tick; a stale non-zero costs one cheap turn. A reader releasing a view does not call `note`, so `owed` stays up until the next housekeeping reclaim recomputes it, and a view held by a long search keeps the index in the housekeeping set until it ends (one extra turn per tick). Harmless, but say so.
3. `stats.total_length` pairs a revision and length from the same `note` call only if no commit lands between the two loads; the pair can be torn. Statistics only.
4. `apply_t` does not call `note`, so `deletes_pending` and queued merges from writes show only after the next refresh/commit. Same tolerance.

### X2b-N4. Read-your-writes depends on `dirty` being cleared after the refresh and set before the ack: keep a test

`ShardEndpoint::fresh` reads `dirty()` without the turn, then takes it and rechecks (`pool.cc:322-341`), and `Core::refresh`/`commit`/`reclaim` clear it after `index->refresh()` (`shard.cc:306-310, 321-329, 1328-1331`). Those are the invariants the guarantee rests on. `test_reads_see_writes_after_refresh` covers the engine; no HTTP-level test does. Add `ryw.py` (reduced) to the harness. The `find_key`-sees-the-buffer change is only visible without `fresh`; every Xapiand read goes through `engine_for_read` (checked all call sites: `database/shard.cc:1496,1529,1554`, `handler.cc:771,1201,1452,1563,1623,1756,1788`, `schema.cc:2776,2842`).

### X2b-N5. Mechanism note and ledger wording

`reader-views.md` Limits says "`find_key` right after an `add` without a refresh finds nothing" but not that this also holds for `Shard::stats()`, `doc_count()` and `declared_fields()` called without `engine_for_read`; list the contract (a caller of a `view()` read must refresh first). The `1000` literal in `manager.cc:235` needs a name and a comment of at most two lines.

## Risk-by-risk results

1. **Read-your-writes: no miss.** `ryw.py` (PUT, then GET and SEARCH by unique term on the same connection and on two other connections, then replace and check new/old tag, then DELETE and check 404 and 0 hits, no explicit refresh):

   | reactors | shards | threads x iterations | misses |
   |---|---|---|---|
   | 1 | 1 | 4 x 300 | 0 |
   | 2 | 1 | 4 x 300 | 0 |
   | 2 | 4 | 6 x 300 | 0 |
   | 1 | 4 | 6 x 300 | 0 |
   | 2 | 2 | 12 x 600, plus 3 bulk RESTORE writers (200 docs, every 4th `?commit`) for 40 s | 0 (7,200 iterations) |
   | 1 | 1 | same | 0 (7,200 iterations) |

   Zero exceptions, zero non-2xx on any run. A miss would have been a blocker; there is none in these configurations. Limits of the probe: one node, macOS, no 3-node cluster.
2. **Close/reopen with readers.** Read: `begin_retire` stops new views (`readers.store(nullptr)`, `index->close()`), `drain_readers` polls `try_finish_close` with a 1 to 50 ms backoff on the host timer, 30 s then `busy`; the index is destroyed only after the last view is released or, on timeout, with strata's keep-alive (`index.hh:615-631`, views stay valid). A leaked reader cannot make a close wait forever (30 s bound, `probe_timeout.cc`), and the reader-side check (`check_read`) touches only atomics, not `core.index` (`shard.cc:417-428`). Churn probe `churn.py` (12 indexes, pool size 3, 2 reactors, 8 clients doing PUT/SEARCH/GET plus a loop of `FLUSH?databases` that closes every idle index, 30 s): 3,816 of each of put/search/get, all 2xx, 51 FLUSH 200, SIGTERM then clean exit in 1.5 s (rc 0). A first run with the clients still hammering during SIGTERM took 42 s to finish ("HTTP is busy" every 5 s): that is the existing graceful drain with clients that reconnect, not a close wait.
3. **Atomics in `Core::note`:** see N3. No harmful stale read found.
4. **Registry sizing:** see B1 and N1. `too_many_readers` does map to 503 (`shard.cc:108-109`) and `test_readers_cap` covers the overload case; the 503 is wrong only for the fan-out case.

## Other checks

- Test stability: `xapiand_test_engine_host`, `xapiand_test_pool`, `xapiand_test_engine_search_strata` each 8 runs on the Release build, 0 failures.
- `OwnedDocView` copies data and values (`doc_view.h:57-76`), so a `get`/`scan` result does not outlive the view it was read from.
- `Shard::scan` and `find_key`/`get` hold a `Viewed` across no `co_await`.
- The proof listed in the ledger was not redone (unit tests on three builds, TSan x9 + 120 s mixed load, E2E 641/641, WAL/crash/shutdown, mutants M1 to M12).

## Conditions to land

1. X2b-B1 closed with a test and the doc update, or the architect rules the model (design question above) and the condition is rewritten.
2. X2b-N1's two numbers in `reader-views.md` and T13 before X3 starts (not before landing).

## Not run

- TSan/ASan/UBSan: no sanitizer build exists in the worktree now and I did not rebuild one; the timeout probe and the churn probe ran on Release only.
- Linux, FreeBSD, x86_64-vs-arm64 differences (the scan probe was x86_64 under Rosetta-class macOS; arm64 records are 128 B).
- A 3-node cluster, replication reads, Lua foreign reads, `restore_database_view`.
- Read-your-writes with a search in flight on the same shard that spans a merge publish (the bulk-writer run covers flush/merge indirectly, not a held view across a merge).
- The docs E2E, WAL, crash and shutdown scripts (the developer's, not redone).
- Benchmarks and the real cost of a scan inside a refresh.
- `src/sortable_serialise.{cc,h}` not opened.

## Architect ruling on the reader registry (X2b-B1, N1, N2)

- Date: 2026-10-30.
- Reviewed: this review's B1, N1, N2 and its probes (`.scratch/review-x2b/fanout*.py`, `scan_cost.cc`, `probe_timeout.cc`); `Xapiand:127ba188b` `src/manager.cc:235`, `src/engine/strata/search.cc:631-656`, `src/engine/strata/shard.cc:184-208, 454, 1476-1490`, `src/engine/strata/viewed.h:38-62`, `src/database/pool.cc:585-602`; `docs/mechanisms/reader-views.md`; `docs/design/phase7c.md` D5, D6 (C9 at `:269-272`); strata `main` `93b5322` `include/strata/core/hazard.hh`, `include/strata/core/index.hh:471, 1266-1343`, `docs/design/shared-reader.md` D4, D5, D6, D9, D10.
- Checked against: Lucene's `SearcherManager`/`ReferenceManager` (a refcount per reader, `incRef` per sub-reader of a multi-reader) and Tantivy's `Searcher` (an `Arc` per searcher over its segment readers), from their design as shared-reader D5 cites it; Elasticsearch's old `action.search.shard_count.limit` (a 400 naming the setting) as precedent for a fan-out 4xx.
- Checked by: `.scratch/arch-registry/registry_arms.cc` (results `results.txt`), clang -O2, macOS x86_64 (i9-9980HK), one run under `quiet light`. Three arms: the X2b global registry (strata `reader_registry`, 50,000 records, 25% claimed), a per-index growable registry (Michael's growable record list: chunks of 4 records, append-only link, a per-thread hint), and a refcount per view (Lucene/Tantivy).
  - Not run: arm64 (128-byte records), Linux, FreeBSD; TSan or the strata litmus on the growable prototype; any Xapiand build or server.

### Ruling

**Per-index hazard registries, growable in chunks ((a) with (c)'s shape); hazards stay; the fan-out limit is the open-index budget, answered 400; no view is held across an await that needs a turn.** The global registry goes. It was never strata's design: shared-reader D4 recommends one registry per pool partition and gives the unsplit node-wide registry as the worst case (D6), and C9 chose the worst case.

| Arm (probe) | Fan-out 10,000 indexes | Owner scan per publish | Hot index, 8 threads | Idle per open index | Verdict |
|---|---|---|---|---|---|
| Global fixed (X2b) | 205 us; 1,010 indexes 503 at capacity 1,000 (B1) | 135 us at 50,000 records, 2.8 ms at 320,000 (N1), every index, grows with the host's load | 8.7 ns (hinted) | 0 (but 3.2 to 20 MB host-wide) | Rejected: scan scales with every reader on the host |
| (a)+(c) per index, chunked | 168 us | 0.001 us at H=4, 0.024 us at 64, 0.12 us at 256 | 8.7 ns/op | 0 until the first reader, then one chunk: 384 B (x86_64), about 640 B on arm64 | **Chosen** |
| (b) one record per search | n/a | n/a | n/a | n/a | Rejected: below |
| (c) global growable | as global | grows with total readers host-wide | as global | as global | Rejected as the scope; kept as the per-index registry's shape |
| (d) epochs | cheap | O(threads) | cheap | 0 | Rejected: below |
| Refcount (Lucene, Tantivy) | 97 us | none (free on last decref) | **348 ns/op** (40x the hazard arm; 177 ns at 4 threads) | 8 B | Rejected for strata's hot-index rows |

Why each loses:

1. (b) One record naming a multi-index snapshot hides each index's view from that index's owner, whose scan looks for its own view pointer. Making owners see through it needs either the composite protected by a second hazard level, with every owner dereferencing every composite on every scan (cost times fan-out), or the composite holding a count on each view, which is the refcount arm. Building the composite also needs each view protected while it is copied. Nothing is saved.
2. (c) as the scope (one growable global) removes the 503 but keeps N1: every commit of every index scans every record any search on the host holds. At the many-indexes target, 10^4 refreshes per second over 50,000 records is 5 x 10^8 record loads per second for nothing.
3. (d) Epochs: one stalled wide search stops every index on the node from freeing a view, so merged-away segments stay on disk node-wide. shared-reader D5's rejection 4 stands; a per-index epoch is a per-index counter, the refcount arm.
4. Refcounts are what Lucene and Tantivy pay, and per index they get what this ruling wants (commit cost independent of other indexes, no fan-out cap). They lose on one hot index: 40x slower at 8 threads on the probe, the case strata's read-while-writing rows gate. A per-index hazard registry has the refcount model's scoping without the shared read-modify-write.

What it overturns in strata's landed design: D4's "one registry per pool partition" becomes "one registry per index by default, a shared one when the host asks", D5's fixed H becomes a cap on a chunked list, and D9's sizing paragraph changes. The protocol (claim, validate, seq_cst scan), D1 to D3 and D6 to D8 are unchanged; Michael's original algorithm is the growable list, so the correctness argument holds with the chunk link stored and loaded seq_cst (a reader's link store precedes its claim, which precedes the owner's publish and scan in the total order).

Against the criteria:

- *Idle cost per open index:* zero until the first reader (strata's reader state is already lazy, `index.hh:2642`), one chunk after.
- *Commit and refresh cost:* O(peak concurrent readers of that index), independent of other indexes and of other searches' fan-out. N1 disappears.
- *Fan-out limit:* no longer the registry's. What a wide search really pins is open indexes: every index it reads stays open and unclosable for the whole search (a held view blocks the idle close, `pool.cc:597`), and the pool is a soft LRU that overflows rather than refuses (`pool.cc:585-602`). So the named limit is `max_indexes_per_search`, at most the open-index budget the descriptor computation sets (`main.cc:338-362`, `database_pool_size`), checked on the resolved shard list (after wildcard and comma expansion) before any view is taken, and answered **400** (`errc::invalid`, the limit and its option in the message). The per-index cap stays a **503**: it now means more concurrent readers of one index than the host runs at once, which is real overload.
- *N2:* see X3 below; the rule makes the cycle unrepresentable rather than timed out.

### What X2b changes before it merges (replaces condition 1)

- **R1. A registry per index.** Each `Core` owns its own registry, as the hostless path already does (`shard.cc:454`, `private_readers`); `DatabasePool::readers_` and the `manager.cc:235` formula go, and phase7c.md C9 (`:269-272`) is rewritten. Until strata's chunked registry lands (X3), the size is fixed at a named `readers_per_index`: the threads that can hold a view of one index at once (reactors plus the pool threads that run reads; `run` is synchronous and dedupes per index, `search.cc:642-656`), times 2 unless the developer shows no read nests a second view of the same index on one thread. Its bytes (records x 64, x 128 on arm64) go in `reader-views.md` Limits with the scan cost from this probe.
- **R2. The registry outlives every guard on it (new, X2b-A2).** A guard's release writes into its registry (`hazard.hh:96-100`), and `Viewed` holds only the guard (`viewed.h:60-61`). With the registry in `Core`, the 30 s drain timeout that drops the entry while a reader holds a view (N2's first half, `pool.cc:550-567`) would make that release a write into freed memory; the global registry hid it. On that path the registry is kept with the views (`keep_forever`), or the guard keeps it alive. A test: drop the shard past the drain with a held view, then release it, under ASan.
- **R3. The fan-out limit.** `max_indexes_per_search` as above, with a test that a search over limit+1 indexes on an idle server answers 400 (not 503), and one at the limit answers 200. Mutant: drop the check; the first test fails.
- **R4. The rule for N2 in `reader-views.md`:** a request takes its views after every await that needs a turn (its `fresh()` refreshes, opens) and, while it holds a view, awaits nothing that takes a turn, opens, or waits for a drain. X2b satisfies it because `run` is synchronous; say so.
- B1's test (capacity+1 indexes, idle, not 503) is subsumed by R3.

### What waits for X3 (before searches suspend)

- **X3-1. strata: the chunked registry** (shared-reader revision 4: D4, D5, D9 as above; `reader()` makes the index its own registry when `index_options::readers` is null). The litmus and TSan stress rerun with chunks appended during claims; an architect re-check of revision 4 only. Xapiand then drops `readers_per_index` for the cap below.
- **X3-2. The per-index cap is D2's in-flight admission cap**, which must exist once searches suspend holding views (one view per index per search). Build the cap; the registry grows to it.
- **X3-3. N2 made structural.** A debug check in the request context (beside `IndexTurn`'s count, D5 R4.3): awaiting a turn, an open or a drain while the request holds a view aborts. And the close and reopen drain off the turn: `begin_retire` under the turn, release it, await the drain; a writer that gets a closing entry's turn already releases and retries (D5 R4.2). Then a search holding A's view and awaiting A's turn cannot meet a close holding A's turn while it waits for A's readers.
- **X3-4. `Shard::view()` takes a lock (new, X2b-A1).** `core.readers.load()` (`shard.cc:1482`) is `std::atomic_load` on a `shared_ptr` (Kronuz `atomic_shared_ptr.h:81-82`): libc++'s global spinlock pool plus a count on one control block per acquire, which is shared-reader D5's rejected arm 1 (the refcount arm costs 348 ns/op at 8 threads here). The acquire path must have no lock and no shared read-modify-write; T13 measures `view()` at 1 and 8 threads on one index.
- **X3-5.** N1's T13 row becomes the per-index scan cost and bytes at 10^4 open indexes, with the cap from X3-2.

### Verdict

**APPROVE WITH CONDITIONS: per-index registries; X2b lands with R1 to R4.** Blocking: R1, R2, R3 (R4 is a doc line). Conditions: X3 does not start suspending searches before X3-1 to X3-3; X3-4 and X3-5 before X3's gate.

Not run: arm64, Linux, FreeBSD; TSan or the strata litmus on the growable prototype; any Xapiand build, test or server; the cost of `view()`'s lock measured inside Xapiand (inferred from the refcount arm, not measured).
