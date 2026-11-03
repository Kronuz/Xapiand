# Road to the single-node release

> Status: CURRENT (2026-11-01). The lead's plan from Phase 7c's state to the first Xapian-free
> release (Phase 8 at `rf = 1`), and the strata lane beside it. Owner rulings are in
> `xapiand-on-strata.md`; Xapiand's own ledgers are in its `docs/status/phase7c*.md` (local master).

## Where it stands

- Phase 7a passed (2026-10-29). Phase 7's gate is split: 7a and 7c, then Phase 8; 7b (cluster) later.
- 7c on Xapiand local master `d25fb70a4`: S1 (tooling, baseline), X1 (Xapian and the cluster
  deleted), X2 and X2b (registry, turns, host mode, chores, reader views, per-index registries).
- Every 7c library step is on its main: task (L1), threadpool WorkPool (L2), http/reactor/http-log
  (L3), prospect sliced search (L4), strata `defer_io` (L5), storage streamed writes (L6), plus the
  compressors decoder fix.
- X3 (views on the reactors) is built and reviewed; one blocker (CLI `--restore` hangs at exit) in
  its fix round.
- The library pin wave of 2026-11-01 (cpp-libraries `docs/UPGRADING-2026-10.md`) is moving
  strata, prospect and Xapiand to the heads; the owner adopted boolean-parser's and datetime's
  result changes.

## Remaining, in order

| Step | What | Gate for | Review |
|---|---|---|---|
| X3 land | B1 fix, the pin wave rebased on it | X4 | lead confirms with the review's probes |
| X3-1 | strata's chunked per-index reader registry (shared-reader revision 4) | searches that suspend at scale | one light architect check (concurrency) |
| X4 | Lua: per-call `_ENV`, yielding hook, caps, the matcher patch (D10, C11-C13) | X6 | one code review |
| X5 | schema, resolver and control plane as coroutines (D11, D13); inventory J | X6 | developer's proof unless a new mechanism appears |
| X6 | delete the offload pools, threads and thread options (D4, D13, D15); destructor asserts; shutdown order (D16); `async_check.sh --strict` in CI | X7 | lead's skim |
| Perf | sliced search's 1.4-2.2% one-slice cost; the keep-alive watch byte's extra read | T12 | performance engineer |
| X7 | the lead runs T1 to T13 on master; gate report `docs/gates/phase7c.md`; Linux and FreeBSD runs owed since 7a (C5) | Phase 8 | architect gate review |
| Phase 8a | replace `sortable_serialise` (GPL) with the 8-byte map (phase-4 D3a) in Xapiand's MIT code; no Xapian reference left; license audit of every dependency (RapidJSON's patch included) | release | security reviewer for the audit |
| Phase 8b | closing benchmark, once: strata against Xapian 2.1.0, and Xapiand alpha.10 (Xapian) against Xapiand on strata end to end, every axis | release | none (published numbers, `quiet exclusive`) |
| Phase 8c | the rename (owner picks the name after a trademark search); changelog; tag; Homebrew; docs site | release | owner |

Before release, also: the barrier warning on non-Apple storage; the docs upgrade page from
alpha.9 and alpha.10 data; the postings fuzz target's memory growth; Detent off http's removed
offload path (coordinated with its agent).

## The strata lane, beside it

1. Cold-tier item 5 (`demote()`, `promote()`, `map_owed`) is built and proven on
   `phase/cold-tier-item5`; one review, then land.
2. Cold-tier step 2: the catalog, CT-R23's read backstop, the T2d wake-cost row.
3. Then the hybrid-search go/no-go run (owner, 2026-10-28): C1 rerun with strata at 1,000-vector
   tenants plus optimization experiments.

## Estimate

About four to five weeks to the release at this week's pace, with the weekly quota as the binding
limit: X3 and the pin wave this week; X4, X5 and X3-1 next; X6, the perf items and X7 after; Phase 8
the last one to two weeks. 7b (the cluster) follows the release.

## Decisions owed by the owner

- The product name (Phase 8c).
- Deleting the 66 archived branches (`branch-triage-2026-10-30.md`).
- `--max-indexes-per-search`'s default (the pool size, 200).

## Carried conditions

- X3-1's C3 (architect check, `docs/reviews/x3-1-chunked-registry.md` on its branch): when
  Xapiand's strata pin moves past X3-1, `StrataReaders::claimed()` calls the registry's new
  non-allocating `claimed()` (not `slot(i)` up to the capacity, which allocates 4096 records per
  teardown), and Xapiand keeps its admission gate at `readers_per_index` until X3-2.
- After the in-family pin wave (owner, 2026-11-02): Xapiand declares only the libraries it includes
  directly, each at the head every library pins, and deletes the early re-declarations it added to
  override stale transitive pins. No override is kept.
- X6 (from X5's fix round): the stop's worst case is the graceful wait plus 70 s (the 35 s bound
  applies twice); cut it to one bound. Remove the dead `--committers` and `num_committers`
  (`search_views.cc:756`) and the `DatabaseCleanup` box in `ARCHITECTURE.md:96`.
- Wide searches (owner, 2026-11-02): replace `--max-indexes-per-search` (the pool size, 200) with
  bounded concurrency, as Elasticsearch does (`max_concurrent_shard_requests`). A search walks its
  indexes in batches of at most K pinned at once (K from the per-reactor reader cap), merging top-k
  as each batch ends; no cross-index snapshot (each index is read as its batch starts; per-index
  read-your-writes holds). A high runaway cap stays (default 100,000 indexes, configurable, 400 above
  it). Builds on the catalog's wake (cold-tier step 2, item 4); lands before the 7c gate, with a row
  measuring a 10,000-index search's latency and peak pinned views.
- Gate prep landed (Xapiand:11e3d82a4, 2026-11-02): owed rows gated, D16 restated, index paths
  refuse escapes, NUL and malformed bytes (SR1; root cause fs:291842c), root routes answer. Follow-ups:
  `HEAD /:stats`, `/:schema` and unknown commands at the root answer 200; `test_quarantine` leaves
  its directories behind; APFS-refused code points give 500; `PUT /.xapiand/node` gives 500; `:wal`
  builds the whole log in memory.
- Index patterns at scale (wide-search SR1, 2026-11-03): patterns resolve by walking the data
  directory under a budget equal to `--max-search-indexes` (100,000 entries), one walk per node at
  a time, so a node with more indexes than the cap can't take a wildcard, and every prefix walks all
  entries. That doesn't fit millions of indexes per host. The fix is a design: resolve names from
  an ordered name index (prefix as a range scan, O(log n + matches)), kept by the catalog's open and
  create paths or a persisted names table, covering nested paths (owner, 2026-11-03: nested indexes
  are the heart of Xapiand; nesting is never refused). Designed with X-POOL step 2 or later; the
  walk stays as the bounded fallback until then, descending into index directories.
- Before X7 (from X6, merged 2026-11-02): read the 31 `async_check --owed` rows and decide D4 to D7
  per row, so `--strict --owed` can gate; restate D16 to the code's order (acceptors close at the
  start of `join()`, after in-flight requests drain; loops stop before indexes close); document that
  an index that cannot close within the 35 s bound now exits 75. `--http-servers`, `--committers` and
  `--fsynchers` are gone; reactors follow `--processors`.
- X4-R1 landed (Xapiand 8ef0f8633): one Lua call holds a reactor at most about 20-26 ms (a
  completing 1 MiB `gsub` 31 ms), and a 16-client script flood leaves light GETs at p99 13 ms. T4's
  1 ms row is measured under the standard mixed load, without adversarial scripts; a script-heavy
  row reports the per-call bound instead. Follow-up: `fuzz_lua` seed 7 disagrees on a cyclic
  document (too large against error), pre-existing on X4.
- 7b input (Detent lead, 2026-11-02): journal and consensus get coroutine drivers (Detent milestone
  M1b, after its M1), with four requirements of ours recorded as binding in Detent's
  `docs/SHARED_LIBRARY_DECISIONS.md` (Detent:a2d999a): `task::task<T>` over `scheduler_ref` with an
  asio adapter; an injectable scheduler, clock and IO for the simulator; bytes per idle group (timers
  on a shared per-reactor wheel, a cost row per design); a run-to-completion entry over the same
  state machine for WorkPool chores. Detent owns both designs; the lead reviews each before code.
- One shape everywhere (owner, 2026-11-02): http's connection loop, handlers, routes and BodyStream
  become `task::task`-based, after reactor's Session does and the reactor owns one scheduler per loop
  (the architect rejected rev 1 for task's missing pending-operation cancellation; http:88f1275).
  The Detent lead owns task's cancellation design (one token with a reason and stop_callback-style
  callbacks, deadlines as token sources, `await_asio` cancellation, an inline resume counted per
  slice) and the reactor and http steps; the lead reviews the task design before code
  (Detent:6702e07). Queued frames are destroyed after stop(), with a zero-live-frames test. Xapiand
  then drops `SearchService`'s bridge and `hreq.on_cancel`. The gate: CPU per request and p99 on
  Linux x86-64 plus one macOS check, allocations and handlers per request no worse than head; the
  "considerable" threshold (proposed: over 5% CPU or 10% p99 after optimizing) awaits the owner.
  L3's review probes rerun on the new API. Architect on rev 2 (http:1ae1390): task REJECTED (CN-L1
  completion during start, CN-L2 cross-scheduler teardown, fixed by four teardown phases), http
  and reactor APPROVED WITH CONDITIONS. Xapiand owes, in the http wave: `take_turn` on
  `lock(token)`, Host forwarding `runs_here` and its root list (C9), chores drained before loop
  teardown, and a `live_frames()` shutdown test under ASan; control_plane's `wake_` cleared by a
  scope guard; `Host::run` never blocks on a spawn after the scheduler closes (a spawn onto a
  closed scheduler destroys the task without calling done). task's cancellation design re-check
  cleared with conditions (task:567c6a6); C1 started; `lock_until`/`lock_for` go with no alias.
