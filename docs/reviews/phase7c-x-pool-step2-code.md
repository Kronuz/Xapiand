> Status: CURRENT (2026-11-03) code review of X-POOL step 2 (`phase7c/x-pool`, head `Xapiand:849dee73c`, code unchanged since `Xapiand:1089c251b`, from `Xapiand:d372d44d4`), built against strata `phase/cold-tier-step2-item5b` (`strata:c39e716`). Verdict: APPROVE WITH CONDITIONS.

# X-POOL step 2 code review: the registry as the catalog's payload

Reviewer: independent code reviewer, fresh context. Subject: the diff against `docs/design/x-pool.md` revision 2 (D1 to D8, D10; XP1, XP2), the ledger `docs/status/phase7c-x-pool.md` ("Design vs code"), the note `docs/mechanisms/index-registry-on-catalog.md`, and the architect's ruling `strata-wt/cold-tier/docs/reviews/cold-tier-xpool-host.md`. Builds: the developer's TSan and ASan trees `.scratch/build-tsan` and `.scratch/build-asan` (object timestamps newer than every source at head; nothing under `src/` changed since `1089c251b`), plus one probe program linked into each. The developer's proof (ctest, E2E, Linux VM, mutants) was not redone beyond the three suites named under "Run".

## Verdict: APPROVE WITH CONDITIONS

One blocker. A frame destroyed while it waits for a pin leaves its waiter linked in the catalog: the open chore later writes into freed memory, and the pin is never released, so the shutdown cannot finish. That is exactly the teardown C24 moved the control roots for, and neither the reorder nor any test covers it. The rest of the concurrency attack held: 6 indexes under a 2-slot catalog (parents and children nested), 8 threads of writes, reads, wide pins, fence-and-reopen, FLUSH and chore commits for 12 s each, and 6 shutdown races, with every acknowledged write present afterwards and no sanitizer report.

## Blockers

**CR1. A root destroyed while it awaits a pin strands the pin and writes into freed memory.** `strata::catalog::pin_awaiter` (`strata/core/catalog.hh:357-383`) has no destructor and no cancellation: `await_ready` links its `waiter` (inside the awaiting coroutine's frame) into the slot's list and counts the pin; `deliver` (`:963`) later reads and writes that waiter. The task library's teardown contract (`task` `docs/design/cancellation.md`, D8 phase 3: "each primitive stays consistent when a frame suspended on it dies") is not met by this awaiter, and Xapiand's `DatabasePool::pin` / `pin_chore` (`src/database/pool.cc:655-700`) await it directly with no shield. Before this branch the wait was a `task::event` flight, which the library makes safe.

Who is exposed: every request root the manager destroys at stop 1 (`manager.cc:1081-1085`) or the control roots it now destroys before stop 3 (`:1100-1106`), when they are suspended in `pin()`, `pin_chore()` or `Shard::ready()`/`reopen()`. A pin waits for a cold open (WAL replay), for a replace to drain, and for a wake. Outside shutdown a cancelled request resumes with an exception, so only teardown is affected.

Probe `destroyed_awaiter` (`.scratch/xp2rev/probe_xp2.cc`, case of that name): per iteration one `Loop` spawns three roots on a cold index (a create pin dropped at once, a create pin held across a wait, a `pin_chore` plus commit), runs the loop for `k*40` microseconds, then does the manager's teardown (`io().stop()`, `IndexChores::cancel_all`, `scheduler().close()`, `destroy_roots()`); 160 iterations over one pool; then `finish()` and `join(30s)`.
- ASan (`out-asan-destroyed_awaiter.log`): `use-after-poison` READ of 8 bytes in `catalog::deliver` from `finish_open` on a pool worker, inside the freed 384-byte frame allocated by `DatabasePool::pin`.
- TSan (`out-tsan-destroyed_awaiter.log`): the same access as a data race with `pin_chore`'s `acquire_impl`; then `join` returns false and `busy()` names `'xp2rev_dest/c2' (open, 1 pin, pinned)`: the pin of a destroyed root is never released, so `catalog.shutdown()` never ends. In the manager that is `abandoned_` and exit 75 ("unfinished writes abandoned") with nothing unfinished; in a debug build the destructor's `patience_` abort.
- One iteration in 160 hit the window with the open of an empty index; a WAL replay widens it to the whole replay.

Consequence for C24: the reorder is right but cannot work while a destroyed waiter strands a pin. Owner of the fix: either strata (an awaiter that survives its frame: heap waiter state, with a delivery to a dead awaiter releasing the pin) or Xapiand (run the `pin_request` in a coroutine the pool owns, outside every root, and have the root await a `task::event`-style slot that tolerates its death, releasing an undelivered pin). Condition: pick one, add a regression test from `destroyed_awaiter` (it is a ready-made one: `join` true and ASan clean), and re-run it under ASan and TSan.

## Conditions

1. Close CR1 with the test above. This is the only item the landing waits on.
2. Rule on CR2 (below) in the design before step 3 builds on it; no code change is required for step 2.

## Findings to carry (not blocking)

**CR2. Pins are uncancellable and untimed; a task that holds one pin and waits for another of the same key waits for itself.** `DatabasePool::pin` takes no cancel token or deadline, and a stale index is replaced only after every other pin on it drains (`catalog.reopen`). Probe `self_wait`: `Shard a` and `Shard b` on one key in one task, `a` marked stale; `a.reopen()` blocks until `b` goes (3 s, then 1 s after `b` released: it is a wait, not a lost wake-up). The ledger records this as a limit. Two places can reach it without a probe: `search_batch` with a repeated endpoint (`handler.cc:1338-1343`) and any request that pins an index it already pins when a fence lands; and the hold-and-wait across keys (a request holding X waits for Y's replace while another holds Y and waits for X's) is possible with opposite pin orders, which the search path avoids by pinning in list order. Fault-only, so not blocking, but a hang here has no timeout or token, only the 30 s shutdown patience. Architect: should `pin()` take the request's token (as `take_turn` does) and should a task's second pin of a key reuse its first?

**CR3. Nothing tests the C24 reorder, and nothing can without CR1's fix.** I agree with the developer that it does not block by itself: a root outlives the stop-2 drain only when the drain hit the deadline, after which stop 3 gets a 1 ms `join` slice. But the property the reorder exists for ("every root that may hold a pin is gone before the shutdown is awaited") is testable at the pool level once CR1 is fixed: destroy a root that holds a pin and awaits `never`, then `finish()` and `join()` must return true. That test, not a manager test, is the one to add; it fails if the reorder's effect (a destroyed root releasing its pin) breaks. (Reversing the moved block in `manager.cc` is then caught by the manager's exit code only in a hand-made stalled-drain case; leave that.)

**CR4. `pin_chore` swallows every exception, `task::cancelled` included** (`pool.cc:696-699`). Harmless for the autocommit (an empty pin ends it), but a cancelled FLUSH is reported as "closed since the snapshot" and moves on to the next key. Narrow the catch to the strata failures `rethrow_pin_failure` knows.

**CR5. Behaviour changes worth stating in the design.** The sweep now closes idle indexes by strata's defaults (`catalog.hh:291`: sweep 1 s, demote after 30 s, close after 600 s) because `make_host` sets none; the old idle close and housekeeping periods are gone. Every open now replays the WAL, so the restore's old "skip when the opener disables the WAL" is gone (ledger item 3; harmless while a restore deletes the log it replaced at its end). FLUSH commits and no longer closes. A 404 scan leaves a negative catalog entry per name until the sweep erases it. None is wrong; the first and last want a number in step 4's budget work.

**CR6. The hostless path is still the adapter's own registry** (`engine::open_local`, `Core::owned`), against D2's "one way to hold an index". Recorded in the ledger as not built; keep it on step 3's list.

## What held

- **Pin lifetimes across chores and reactors.** `Shard` owns its `IndexPin` (`shard.cc:1234`); `lock_shard` has no second pin. `Turn::release` checks the index in under the turn, and `check_in` is `noexcept`. Probe `churn` (8 threads, `DatabasePool(2)` over 6 indexes so the sweep and replaces close and reopen continuously, 12 s): TSan 81 opens, 56 replaces, 288 operations; ASan 72, 52, 284; 0 reports. (Operation counts are low because the sanitizers are slow; the replaces are the point.) Invariant checked on every read, wide read and reopen: document count at least the writes acknowledged before the pin was taken. No violation; afterwards every index in a fresh pool holds exactly its acknowledged documents.
- **The reopen and close race.** Included in `churn`: a pin marked stale through `retry_read` and `reopen()` while other threads read and write the same key. No loss, no deadlock with ascending-order wide pins (`WIDE_RANDOM=1` pins in random order and is the hold-and-wait of CR2; not run to completion as a gate).
- **Shutdown under in-flight work, autocommit-style commits and FLUSH.** Probe `shutdown_race`: six rounds, 8 workers (writes with commit, reads, wide pins, fence-and-reopen, `flush()`, resident `pin_chore` plus commit), `finish()` then `join()` at 300 to 1050 ms. Every round: `join` true, all workers got `not_available`, no other error, every acknowledged write present on reopen. TSan and ASan, 0 reports.
- **Nested indexes.** The same two probes with `NESTED=1`: `p`, `p/c`, `p/c/d`, `p/c/e`, `q`, `q/c` pinned together, opened, replaced and closed under churn. TSan and ASan, 0 reports, no loss. `Endpoint(key).path` is idempotent for a normalised key (probe `endpoint_identity`; only a leading `/`, `./`, a doubled or trailing `/`, `:` and `?` change, and none survives into a catalog key).
- **Wide search through the pins.** The wide pins in `churn` pin four keys at once in ascending order over a catalog whose `soft_open` is 2: no pin waits for capacity (probe `capacity`: 12 keys held by two threads in opposite orders under `DatabasePool(2)` across two sweeps, 12 opens, nothing refused or waited). The missing-index skip is the developer's `test_a_search_batch_skips_a_missing_index`; I read `search_batch` (`handler.cc:1347-1363`) and the skip is inside the per-shard read, so it also survives the retry loop.
- **Resident pins and the idle close.** Every resident pin is scoped to one commit or one request pin (`autocommit`, `flush`, `pin()` in a finished pool); none is held across a wait that depends on another pin, so none can block the sweep forever. A pin taken in a finished pool is `not_available` unless the index is already open or owed.
- **Design decisions followed.** D1 (one catalog, `H = ShardEndpoint`), D3, D4 (`check_no_view_held` in `pin`, `take_turn`, `Shard::reopen`), D5, D6 (`open_payload` replays with chores inline), D7 (`commit_blocking` asserts the free turn), D10 as written. The ledger's "Design vs code" list is complete as far as I found; no scope beyond the design except the step 3 pieces the lead assigned.
- **Tests that could pass without the code.** I found none: the new tests name the mutant that kills them (ledger), and `test_the_pool_waits_for_a_pin_released_during_its_end` kills the destructor mutant.

## Run

Probes (all kept, in `.scratch/xp2rev/`): `probe_xp2.cc` (cases `endpoint_identity`, `capacity`, `self_wait`, `destroyed_awaiter`, `churn`, `shutdown_race`; `NESTED=1`, `WIDE_RANDOM=1`), `mk.sh tsan|asan` (links it against the developer's trees), `run.sh`, `run2.sh`, `run3.sh`, logs `out-*.log`. Built with `quiet light xp2rev`.

| Run | Result |
|---|---|
| `probe-tsan`: endpoint_identity, self_wait, churn, shutdown_race | pass, 0 reports |
| `probe-tsan` capacity | pass; 2 TSan reports are my probe's own `testh::checks` counter, not the branch |
| `probe-tsan` destroyed_awaiter | race in `finish_open`/`deliver`; `join` false; abort (CR1) |
| `probe-asan`: churn, shutdown_race, capacity | pass, 0 reports |
| `probe-asan` destroyed_awaiter | use-after-poison in `deliver` (CR1) |
| nested churn and shutdown_race, TSan and ASan | pass, 0 reports |
| `build-tsan` `xapiand_test_pool` / `test_reactor_requests` / `test_node` | 231 / 59 / 106 checks pass, 0 reports (`report_thread_leaks=0`, `die_after_fork=0`) |

## Not run

The docs E2E, `graceful_shutdown_check.sh`, `path_boundary_check.sh`, `async_check`, and the Linux and release builds (the developer's ledger covers them; CI owns the platforms). `test_wide_search`, `test_engine_host` and `test_blocking_guard` (the developer's TSan and ASan logs stand). Any mutant of the developer's. A manager-level shutdown with a request suspended in a pin (CR1 is shown at the pool and scheduler level with the manager's own teardown calls, not through `XapiandManager`). `WIDE_RANDOM` to completion (CR2's cross-key cycle is argued, not demonstrated). Fault injection through strata's faultfs beyond the developer's tests. gcc and FreeBSD.
