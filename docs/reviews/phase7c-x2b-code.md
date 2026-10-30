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
