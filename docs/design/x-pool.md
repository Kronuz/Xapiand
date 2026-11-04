> Status: DRAFT (2026-11-02) revision 2, X-POOL, Xapiand on strata's catalog: the architect's rulings on F1 to F6 (`strata/docs/reviews/cold-tier-xpool-host.md`, approve with conditions) folded in; step 1 (the seam) built on `phase7c/x-pool`, step 2 planned against item 5b's host interface. Workstream `phase7c/x-pool`, ledger `docs/status/phase7c-x-pool.md`.

# X-POOL: the registry becomes the catalog's payload

## Summary

strata's catalog (`strata::catalog<Index, H>`, `include/strata/core/catalog.hh`; design
`strata/docs/design/cold-tier.md` revision 7, D4, D5 and "Steps"; note `docs/mechanisms/catalog.md`)
owns a partition's non-cold indexes, their slots, pins and transitions. X-POOL makes Xapiand's
`DatabasePool` one catalog over `prospect::index` whose payload `H` is what is left of a
`ShardEndpoint`: the turn, the `WorkGroup`, the positions and the blob storage. `refs`
(`src/database/pool.h:194`) become pins held by reads and writes alike, the idle close goes (the
catalog's sweep closes), the commit becomes the catalog's commit chore, and the turn handles
`map_owed` beside `flush_owed`.

Step 1 (the seam) needs nothing new from strata. Steps 2 to 4 build against item 5b
(`phase/cold-tier-step2-item5b`, stacked on item 5), which gives the catalog the host interface the
architect ruled: open intents, `open_payload`, `reopen(pin&&)`, counted host files and `snapshot()`.

## Decisions

**D1. One catalog per `DatabasePool`**, over `prospect::index`, with `H = IndexEntry`. `IndexEntry` keeps
the turn (`task::async_mutex`), `IndexChores`, `revision`, `applied_`/`committed_`, the commit flight,
`last_flags_`, the blob storage and `next_volume_`. It need not move: `open_payload` builds it in place
(XH3). Gone from `ShardEndpoint`: the `lru` map, `State`, `OpenFlight`, `closed_`, `refs`, `renew_time`,
`finished`, `stale_`; the word holds them. Gone from the adapter: `StrataReaders` and its `AdmissionGate`
(XH2). The catalog's one `index_options` sets `readers = nullptr` and `reader_records = readers_per_index`,
so each index owns its registry (strata R4.1), and `too_many_readers` is the per-index 503 that
`views_refused()` counts.

**D2. The catalog owns the index and its IO; the adapter borrows them.** `engine::strata::Core` holds a
pointer to the pinned `prospect::index` for the pin's life (I7). The adapter's own registry (`registry`,
`Entry`, `Handle` in `src/engine/strata/shard.cc`), `drain_readers`, the `handles` vector, `fenced`,
`retiring`, `epoch` and `mark_stale` go: the catalog closes only at zero pins, and a fenced index is
replaced by the catalog. A call that throws `index_fenced` hands its pin to `reopen(std::move(pin))` and
retries by key; `corrupt_segment` runs `heal()` under the turn. The hostless path (tests, the CLI) keeps a
private catalog of its own, so there is one way to hold an index.

**D3. One seam.** `IndexPin` in `src/database/pool.h` wraps the pin and is the only Xapiand type that will
name the catalog. `co_await pool.pin(endpoint)` is a request pin and a use (one per request, by key);
`co_await pool.pin_chore(endpoint)` is a `resident` chore pin: it never opens, and an index that is not
open gives an empty pin. `lock_shard` holds an `IndexPin` and is taken with `co_await lk_shard.pin()` at
all of its callers; the autocommit takes `pin_chore`. Step 2 adds the write intent: a pin made with
`DB_CREATE_OR_OPEN` asks `open_intent::create`, any other `open`. Wide-search's
`DatabaseHandler::search_batch` pins its batch through `IndexPin` once it lands.

**D4. Pin before view.** A request awaits every pin it will read before it takes any view, since a pin
may open or wake. `DatabasePool::pin` calls `engine::check_no_view_held`, so a pin taken under a view
aborts in debug builds (`reader-views.md`, "The rule").

**D5. Writes pin too.** `ensure_open` goes (the pin opens); `take_turn` takes the turn of a pinned
entry. A turn is held only under a pin, so at `{0, ...}` the turn is free, which D7 asserts. The turn's
release calls the index's `check_in()`, which publishes finished merges and frees released views (F6's
ruling: the turn holder's, never the catalog's on a pinned slot).

**D6. The open.** `resolve(opening)` makes the counted `PosixIO` on `<path>/strata` and throws
`strata::index_not_created` for a missing directory unless `opening.create`; `open_index` creates when
`create` and the index is absent. `open_payload(opening, index, h)` emplaces `IndexEntry`, opens the blob
storage with its descriptors reserved on `opening.counts` (F5), and replays the WAL into the index with
chores inline; a throw fails the open, destroying H, then the reader, the index and the IO. The slot is
`opening` throughout, so no pin sees the index before the replay ends. Restore is a create pin with the
WAL off (XH5).

**D7. The commit chore** is `ShardEndpoint::do_commit`'s body: blob fsync, engine commit, WAL COMMIT
line. The catalog runs it at zero pins (sweep, bound close, replace close, shutdown); it takes the turn
with `try_lock`, which cannot fail there (asserted).

**D8. The idle close and the housekeeping pass go.** `begin_idle_close`, `finish_idle_close`, `cleanup`'s
idle path, `housekeeping_pass` and their timers are deleted: the sweep closes by its policy, the catalog's
demote and close chores check in, and the turn's end checks in (D5). The autocommit re-arms while a commit
leaves merges pending, which covers a read-only busy index. `FLUSH` takes `snapshot()`, pins each key
`resident`, and commits; a key closed in between closed committed. `busy()`, `count()` and
`dump_databases()` read the snapshot and the catalog's stats.

**D9. `map_owed`.** `apply_t` answers `verdict::map_owed` as it answers `flush_owed`: a query chore
runs `promote()` under the turn, then the same transaction is applied again. strata ends the answer
(CT-R22's owner bit); Xapiand bounds its loop at two owed answers per transaction of any kind and
raises `errc::internal` past it, so a strata regression is an error, not a spin.

**D10. Shutdown.** `DatabasePool::join`, on the main thread that may wait (`pool.cc:807`), waits for
`catalog.shutdown()`, which runs on the executor, never a reactor; a key whose commit failed is logged,
and the WAL replays it at the next open.

**D11. The budget.** One `resource_budget` per process. `soft_open` is the pool size (`main.cc`'s
descriptor computation), at most the derived cap; `soft_hot` is strata's default. CT-R39: the cap on
indexes one search holds at once stays at most `soft_open`: today `max_indexes_per_search`
(`main.cc:487`), after wide-search lands its `--concurrent-search-indexes` (K), clamped the same way.

**D12. The gate's assertion.** Debug builds assert, after every turn and at unpin, that a `defer_io`
index's `unleased_maps()` is zero; the docs E2E runs on such a build.

**D13. What the catalog changes for a user** (the step 2 review's CR5), each a decision:
1. The sweep closes an idle index by strata's times: a pass each second, a hot index demoted after 30 s idle and
   closed after 10 min (`IndexSweep`; a node passes none). The old idle close took an hour, a minute when the pool
   overflowed. Step 4's budget work sets the numbers.
2. Every open replays the WAL, the restore's included; the restore no longer skips a log because its flags disable
   the WAL, and it still deletes the log of what it replaced at its end.
3. `FLUSH` commits every open index and no longer closes it; the reference documents it as a commit, and the sweep
   closes what idles.
4. A request for an index that does not exist leaves a negative catalog entry under its name until the sweep erases
   it (`negative_ttl`, 10 s); step 4's budget work bounds them.
None narrows what a request can do: each changes when resources are let go, not an answer.

## Rulings on the host interface

`strata/docs/reviews/cold-tier-xpool-host.md` (2026-11-02) ruled the six gaps revision 1 raised; strata
builds them as item 5b (S10 to S16), which lands before X-POOL's step 2.

| Gap | Ruling | Where it lands here |
|---|---|---|
| F1 create intent | `open_intent {open, create, resident}`; only `create` passes an absent negative entry | D3, D6 |
| F2 payload before index | Rejected: the index owns its registry; `StrataReaders` and the gate go | D1 |
| F3 a step at open | `open_payload(opening, Index&, optional<H>&)` builds H in place and replays | D6 |
| F4 reopen on request | `reopen(pin&&)`, holders drain in `closing`; restore needs only `create` | D2, D6 |
| F5 host descriptors | `opening.counts` reaches the payload; public reservation helpers | D6 |
| F6 per-index host work | Rejected as a hook: check-ins, `resident` pins, `snapshot()` | D5, D8 |

## Gate rows

Branch rows are functional; the lead measures.

| Row | Target | Procedure |
|---|---|---|
| **XP1** pin before view | A pin awaited under a held view aborts (debug); a pin after the view is let go is fine; every request path pins before its views | `test_reactor_requests` (the view rule, "a pin"); the docs E2E on an asserts build; mutant: no check in `pin` |
| **XP2** a pin opens or wakes | A cold key's request pin opens, replays the WAL, and hands the pin with the replayed documents readable; an `owed` index's request pin wakes it, `unleased_maps()` zero; a pin after `reopen` of a fenced index reads the replayed documents | `test_pool`; demote, pin and read with `faultfs::calls()` unchanged across `apply`; mutants: deliver the pin before the replay; no `reopen` on `index_fenced` |
| **XP3** the idle path gone | No idle-close code path; an unpinned idle index closes by the sweep only, a pinned one never | `test_pool` with the sweep at 1 kHz and a held pin; mutant: unpin early (the pinned index closes) |
| **XP4** commit as a catalog chore | A write left uncommitted is committed (WAL COMMIT line, revision + 1, blobs synced) by the sweep's close and by `shutdown()` | `test_pool`; mutant: the default chore (`index.commit()`) loses the WAL line |
| **XP5** `map_owed` | A write to a demoted `defer_io` index applies after one `promote()`; a third owed answer is `errc::internal` | `test_pool` with a strata stub that answers owed forever; mutant: no retry |
| **XP6** sanitizers | Zero TSan, ASan, UBSan reports | separate Linux builds on the AWS lease, `ctest`, the docs E2E (`npx newman@6.2.3`), `graceful_shutdown_check.sh`, `path_boundary_check.sh` |
| **XP7** no blocking | 0 owed | `async_check --strict --owed --extern strata=<pin>` |

XP2 and XP4 run with `defer_io` and a fault that fences (C23). Thread mutants die under TSan in the same
runs.

## Steps

1. **The seam** (D3, D4): `IndexPin` over today's `refs`, `lock_shard` awaited, the pin-before-view
   check, the autocommit's resident pin. XP1, XP6, XP7. Built.
2. **The registry as `H`** (D1, D2, D5, D6), against item 5b: `IndexEntry` built by `open_payload`, the
   adapter borrowing the pinned index, `StrataReaders` and the adapter's registry deleted, create intent
   from the flags, `index_fenced` to `reopen`. XP1, XP2.
3. **Commit chore, idle close and housekeeping gone, shutdown, FLUSH by snapshot** (D7, D8, D10). XP3,
   XP4.
4. **`map_owed`, the gate assertion, the budget** (D9, D11, D12): XP5; then XP6 and XP7 on the whole.
