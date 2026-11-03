> Status: DRAFT (2026-11-02) X-POOL, Xapiand on strata's catalog; stopped for the lead's skim at six gaps in the catalog's host interface (F1 to F6), which are strata's to rule. Workstream `phase7c/x-pool`, ledger `docs/status/phase7c-x-pool.md`.

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

Built against strata `phase/cold-tier-step2-item4` (strata:1b0c382) and read against item 5
(`phase/cold-tier-step2-item5`, strata:a748805). The catalog as built cannot host Xapiand's open, for
the six reasons in "Gaps"; D1 to D12 assume the recommended answers.

## Decisions

**D1. One catalog per `DatabasePool`**, over `prospect::index`, with `H = IndexEntry`. `IndexEntry` keeps
the turn (`task::async_mutex`), `IndexChores`, `revision`, `applied_`/`committed_`, the commit flight,
`last_flags_`, the blob storage and `next_volume_`, the per-index `StrataReaders` (hazard records and
admission gate, `reader-views.md` 2) and the engine adapter's `Core`. Gone from `ShardEndpoint`: the
`lru` map, `State`, `OpenFlight`, `closed_`, `refs`, `renew_time`, `finished`; the word holds them.

**D2. The catalog owns the index and its IO; the adapter borrows them.** `engine::strata::Core` holds a
pointer to the pinned `prospect::index` for the pin's life (I7: nothing is kept past a pin). The adapter's
own registry (`registry`, `Entry`, `Handle` in `src/engine/strata/shard.cc`), `drain_readers`, the
`handles` vector and the `epoch` go: the catalog closes only at zero pins and zero maps, so no close
waits for readers and no index is replaced under a pin. The hostless path (tests, the CLI) keeps a
private catalog of its own, so there is one way to hold an index.

**D3. One seam.** `IndexPin` in `src/database/pool.h` wraps `catalog::pin` and is the only Xapiand
type that names the catalog. `co_await pool.pin(endpoint, flags)` is a request pin and a use (one per
request, by key); `pool.repin(pin)` re-pins by handle and is not a use (CT-R31); `pool.pin_chore(...)`
serves timers, autocommit and housekeeping. `lock_shard` holds an `IndexPin`, and taking it becomes an
await (29 callers); wide-search's `DatabaseHandler::search_batch` pins its batch through it.

**D4. Pin before view.** A request awaits every pin it will read before it takes any view, since a pin
may open or wake. `engine::check_no_view_held` moves into `pool.pin` (it stays in `take_turn`), so a
pin taken under a view aborts in debug builds (`reader-views.md`, "The rule").

**D5. Writes pin too.** `ensure_open` goes (the pin opens); `take_turn` takes the turn of a pinned
entry. A turn is held only under a pin, so at `{0, ...}` the turn is free, which D7 asserts.

**D6. The open chore** resolves `<path>/strata` to a counted `PosixIO`, opens without creating
(F1), makes the payload before the index (F2), then opens the blob storage and replays the WAL into
the index with chores inline (F3) before the slot turns `open`. A failure fails the open and its
waiters; the catalog's backoff applies.

**D7. The commit chore** is `ShardEndpoint::do_commit`'s body: blob fsync, engine commit, WAL COMMIT
line. The catalog runs it at zero pins (sweep, bound close, shutdown); it takes the turn with
`try_lock`, which cannot fail there (asserted).

**D8. The idle close goes.** `begin_idle_close`, `finish_idle_close`, `cleanup`'s idle path and the
timer that drives it are deleted; the sweep (item 5) closes by its policy. `FLUSH` commits every open
index and closes none (closing is the sweep's).

**D9. `map_owed`.** `apply_t` answers `verdict::map_owed` as it answers `flush_owed`: a query chore
runs `promote()` under the turn, then the same transaction is applied again. strata ends the answer
(CT-R22's owner bit); Xapiand bounds its loop at two owed answers per transaction of any kind and
raises `errc::internal` past it, so a strata regression is an error, not a spin.

**D10. Shutdown.** `DatabasePool::join`, on the main thread that may wait (`pool.cc:807`), waits for
`catalog.shutdown()`, which runs on the executor, never a reactor; a key whose commit failed is logged, and the WAL replays it at the next open.

**D11. The budget.** One `resource_budget` per process. `soft_open` is the pool size (`main.cc`'s
descriptor computation), at most the derived cap; `soft_hot` is strata's default. CT-R39: the cap on
indexes one search holds at once stays at most `soft_open`: today `max_indexes_per_search`
(`main.cc:487`), after wide-search lands its `--concurrent-search-indexes` (K), clamped the same way.

**D12. The gate's assertion.** Debug builds assert, after every turn and at unpin, that a `defer_io`
index's `unleased_maps()` is zero; the docs E2E runs on such a build.

## Gaps in the catalog's host interface (for the architect)

The host struct (`catalog.hh`, item 4 :176-196, item 5 :201-217) gives `resolve(key, io_counts)`,
`open_index(IO&, options)`, then `make_payload(key)`. Xapiand needs:

| Gap | Why Xapiand needs it | Recommendation |
|---|---|---|
| **F1** Create intent | A read of a missing index answers 404 and creates nothing; a write creates it (`has_db_create_or_open`, `src/database/shard.cc:494`). A failed open is a negative entry (10 ms doubling to 10 s), so after ten reads of a missing index a write that creates it fails for up to 10 s. No host-side workaround: the slot's `retry_at` gates every pin | A pin carries an open mode (`open`, `create`); a slot opening for any `create` pinner creates; `index_not_created` is no negative entry, or a `create` pin passes it |
| **F2** Payload before index | Each index's `options.readers` names its own hazard registry (`src/engine/strata/shard.cc:466`), which must outlive the index and every guard; `index_options` is one per catalog and `H` is made after the index and destroyed before it (`slot` member order) | `make_payload(key, io_counts)` before `open_index(IO&, options, H&)`; `H` declared before `io` and `index` in `slot`, so it goes after them |
| **F3** Open-time host step | The WAL replays into the index before anyone reads it (`ShardEndpoint::open_index_blocking`, `src/database/shard.cc:482-537`); replay needs the index and `H` | `after_open(H&, Index&)` in the open chore; a throw fails the open |
| **F4** Reopen in place on request | A fenced index (`mark_stale`, `pool.cc:282`) and a restore (`src/database/shard.cc:1354`, `:1401`) replace the index while the requester holds a pin | `request_reopen(handle)`: the slot closes at its last unpin and reopens in place for waiters (CT-R30's row), or the sweep does it; pins meanwhile wait as on `closing` |
| **F5** Host descriptors on the slot | The blob storage keeps a descriptor per open index (`writable_storage`); 50,000 open indexes would put 50,000 descriptors outside the budget's reserve | `make_payload` receives the slot's `io_counts`, so `H` reserves what it opens and evicting the slot names it |
| **F6** Host work per open index | Housekeeping (`housekeeping_pass`, `pool.cc:719`) and autocommit visit open indexes; the catalog exposes no enumeration (only test accessors) | A sweep hook `housekeeping(H&, Index&)` run as a chore under a chore pin, or `for_each_open` |

F1 to F3 block the first slice (the registry as `H`); F4 to F6 block the rest. One change to the host
struct covers all six; it belongs with item 5 or as an item of its own, reviewed with the catalog.

## Gate rows

Branch rows are functional; the lead measures.

| Row | Target | Procedure |
|---|---|---|
| **XP1** pin before view | A pin awaited under a held view aborts (debug), and every multi-index read pins all before its first view | `test_pool`: a search over 3 indexes records pin and view order; mutant: take each view right after its pin (the order check fails) |
| **XP2** a pin opens or wakes | A cold key's request pin opens, replays the WAL, and hands the pin with the replayed documents readable; an `owed` index's request pin wakes it, `unleased_maps()` zero | `test_pool`: write, close by the sweep, pin and read; demote, pin and read with `faultfs::calls()` unchanged across `apply`; mutant: deliver the pin before the replay |
| **XP3** the idle path gone | No idle-close code path; an unpinned idle index closes by the sweep only, a pinned one never | `test_pool` with the sweep at 1 kHz and a held pin; mutant: unpin early (the pinned index closes) |
| **XP4** commit as a catalog chore | A write left uncommitted is committed (WAL COMMIT line, revision + 1, blobs synced) by the sweep's close and by `shutdown()` | `test_pool`; mutant: the default chore (`index.commit()`) loses the WAL line |
| **XP5** `map_owed` | A write to a demoted `defer_io` index applies after one `promote()`; a third owed answer is `errc::internal` | `test_pool` with a strata stub that answers owed forever; mutant: no retry |
| **XP6** sanitizers | Zero TSan, ASan, UBSan reports | separate builds (Homebrew LLVM), `ctest`, the docs E2E (`npx newman@6.2.3`), `graceful_shutdown_check.sh`, `path_boundary_check.sh` |
| **XP7** no blocking | 0 owed | `async_check --strict --owed --extern strata=<pin>` |

Thread mutants for XP1 to XP4 die under TSan in the same runs.

## Steps

1. The seam (D3, D4): `IndexPin` over today's `refs`, `lock_shard` awaited, the pin-before-view check.
   Buildable now; churn-free once the catalog lands behind it.
2. The registry as `H` (D1, D2, D5, D6), after F1 to F3: XP1, XP2.
3. Commit chore, idle close gone, shutdown, housekeeping (D7, D8, D10), after item 5 and F4 to F6: XP3, XP4.
4. `map_owed` and the gate assertion (D9, D12), the budget (D11): XP5; then XP6, XP7 on the whole.
