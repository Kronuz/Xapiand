# The stash / scheduler / debouncer stack

One small, lock-free data structure underpins a surprising amount of Xapiand:
the logger's dedicated background thread, every debounced fsync and commit, and
the replication triggers all ride the same scheduling engine. This document
pins down what that engine is, who uses it, and how cleanly it could be lifted
out into standalone ("solo") libraries.

It's three layers, each in one header, stacked cleanly:

```
  stash.h        a lock-free hierarchical slot store (a timer-wheel primitive)
      ▲
  scheduler.h    a 24-hour multi-resolution timer wheel + a thread that runs due tasks
      ▲
  debouncer.h    per-key throttle / debounce on top of the scheduler
```

Include graph (verified): `stash.h` is included only by `scheduler.h`;
`scheduler.h` only by `debouncer.h` and `logger.h`; `debouncer.h` by
`storage.h`, `database/handler.h`, `manager.h`, and `server/discovery.h`. So the
whole engine has exactly **two** direct entry points — the logger and the
debouncer — and everything else goes through the debouncer.

---

## Layer 0 — `stash.h`: the lock-free slot store

A hierarchical, append-mostly store addressed by an unsigned integer key, built
from three templates:

- **`Stash<T, Size>`** (`stash.h:128`) — the primitive: a chunked, lazily-grown
  array of `std::atomic<T*>`. Slots past `Size` spill into a linked `Data` node,
  and both chunks and next-nodes are allocated with `compare_exchange_strong`
  (`stash.h:186,202`), so concurrent producers never lock.
- **`StashSlots<T, Size, CurrentKey, Div, Mod, Ring>`** (`stash.h:236`) — a keyed
  level. A key maps to slot `(key / Div) % Mod` (`stash.h:255`); `T` is itself a
  `Stash`, so levels nest to give multiple resolutions. `next()` walks a key
  window in one of three modes — *walk* (consume), *peep* (look-ahead, no
  mutation), *clean* (GC emptied slots) — recursing into the nested level
  (`stash.h:266`). `put()`/`add()` insert at a key, and `add()` throws
  `out_of_range("stash overflow")` if the key is beyond the wheel's span
  (`stash.h:371`).
- **`StashValues<T, Size, CurrentKey>`** (`stash.h:387`) — the leaf: an
  append-only list with an atomic write cursor (`atom_end`) and separate `walk`
  and `clean` read cursors, so consumption and garbage collection chase each
  other without locking (`stash.h:479,408`).

`StashContext` (`stash.h:47`) carries the operation, the key window
(`begin_key`/`end_key`), and the atomic first/last-valid-key bounds that let the
walk skip empty ranges.

**Dependencies:** `<array>`, `<atomic>`, `<cassert>` — and `log.h` purely for the
`L_STASH` / `L_DEBUG_HOOK` / `L_EXC` trace macros and the `colors.h` constants.
Nothing else. Stub those macros and the file stands alone.

> **Cleanup found while reading:** the `Ring` and `CurrentKey` template
> parameters are declared but **never referenced** in the bodies (`stash.h:235`,
> `386`). Ring-wrap happens implicitly via the `% Mod` in `get_slot`, and the
> "current key" is always passed in through the context. Both can be deleted,
> which also drops the `&now` function-pointer plumbing.

## Layer 1 — `scheduler.h`: the timer wheel and its thread

`SchedulerQueue` (`scheduler.h:95`) nests four `StashSlots` levels over a
`StashValues` leaf into a single **24-hour wheel** keyed on nanoseconds since the
steady-clock epoch (`scheduler.h:106-110`):

```
  4800 × 18 s  (ring)   →  36 × 500 ms  →  10 × 50 ms  →  50 × 1 ms  →  task list
       └── 4800 × 18 s = 86 400 s = exactly 24 h of horizon, at 1 ms granularity
```

A task scheduled more than ~24 h out overflows the wheel (caught and logged,
`scheduler.h:158`). The queue exposes `peep` (earliest upcoming, no mutation),
`walk` (drain everything due now), `clean` (GC slots older than a minute), and
`add` (`scheduler.h:121-161`).

On top sit:

- **`ScheduledTask<Scheduler, Impl, policy>`** (`scheduler.h:60`) — a CRTP,
  `enable_shared_from_this` task with a `wakeup_time` and atomic
  created/cleared timestamps. `clear()` cancels it with a single CAS on
  `cleared_at` (`scheduler.h:85`); `operator bool()` reports "not yet cleared."
  This is what makes a scheduled callback *cancellable* — the debouncer leans on
  it heavily.
- **`BaseScheduler<…>`** (`scheduler.h:165`) — a `Thread` whose loop proposes a
  wakeup (30 s when idle, 100 ms when tasks pend), `peep`s for anything sooner,
  then sleeps on a condition variable until that time **or** an `add()` notify
  wakes it (`scheduler.h:202-251`), then `walk`s the due tasks and `clean`s.
  One mutex, one condvar, a lock-free queue.
- **`Scheduler<Impl, policy>`** (`scheduler.h:296`) — runs each due task
  **inline** on the scheduler thread (`task->operator()()`, `scheduler.h:333`).
  This is what the logger uses.
- **`ThreadedScheduler<Impl, policy>`** (`scheduler.h:341`) — owns a `ThreadPool`
  and **dispatches** each due task to a worker (`scheduler.h:406`), so a slow
  task can't stall the wheel. This is what the debouncer is built on.

**Dependencies:** `stash.h` (its own), `thread.hh` (the `Thread` CRTP +
`ThreadPolicyType`), `threadpool.hh` (only `ThreadedScheduler` needs it),
`log.h` (stubbable), and `<chrono>/<mutex>/<atomic>`.

## Layer 2 — `debouncer.h`: per-key throttle / debounce

`Debouncer<Key, Func, Tuple, policy>` (`debouncer.h:41`) extends
`ThreadedScheduler` and keeps a per-key status map. Rapid touches of the same key
collapse into a single eventual call to `func`. The timing has three knobs and
one nice trick:

- a fresh key fires after `debounce_timeout`; subsequent touches push it out by
  `debounce_busy_timeout` (`debouncer.h:226-235`);
- but never past a **randomized force window**, `now + random(min_force,
  max_force)` (`debouncer.h:223`), so a key that's touched forever still fires —
  the randomization spreads a thundering herd of due keys across time;
- after firing, a `throttler` task enforces a `throttle_time` floor before the
  key can fire again (`debouncer.h:153-187`).

`debounce()` / `delayed_debounce()` are the API; `make_debouncer` /
`make_unique_debouncer` / `make_shared_debouncer` (`debouncer.h:266-286`) deduce
the argument `Tuple` from the callback via `callable_traits`.

**Dependencies:** `scheduler.h` (its own), `callable_traits.hh` (self-contained),
`random.hh` (three free functions), `log.h` (stubbable).

---

## Who rides the engine

- **The logger** (`logger.cc:456`) owns a singleton
  `Scheduler<Logging, ThreadPolicyType::logging>` named `"LOG"` — the dedicated
  logging thread. Error-level and delayed log messages are `ScheduledTask`s on
  it; the delayed-then-`unlog` latency pattern is exactly `ScheduledTask::clear`
  in action.
- **Debouncers** (built via `make_debouncer`) drive the background coalescing in:
  - `storage.h` — the asynchronous fsync (`fsyncher`);
  - `database/handler.h` — committers / updaters;
  - `server/discovery.h` — the `DB_UPDATED` multicast debounce and the
    schema/settings/primary updaters;
  - `manager.h` — manager-level updaters.

So "the stash thread" the engine is built around is really *two* uses of one
mechanism: the logger's inline scheduler thread, and a family of threaded
debouncers for every rate-limited background activity.

---

## Extraction into solo libraries

The stack is unusually clean to lift because the layering is strict and the
coupling is shallow. The natural decomposition is **three tiers**, each useful on
its own:

### 1. `stash` — header-only, the crown jewel

The lock-free hierarchical slot store. This is the genuinely novel, publishable
piece. To extract:

- Replace the `L_STASH` / `L_DEBUG_HOOK` / `L_EXC` macros and `colors.h`
  constants with no-op stubs (or one optional `STASH_TRACE(...)` hook).
- Delete the vestigial `Ring` / `CurrentKey` template parameters.

Result: a single header depending only on `<array>/<atomic>/<cassert>`.
**Difficulty: Easy. Value: High.**

### 2. `timer-wheel-scheduler` — depends on `stash`

`ScheduledTask` + `SchedulerQueue` + `Scheduler` / `ThreadedScheduler`. To
extract:

- Stub the `L_*` macros (same as tier 1).
- Decide the thread story: either bundle trimmed copies of `Thread` /
  `ThreadPool`, or retarget the loop onto `std::jthread` + a small pool. The
  inline `Scheduler` needs only a thread; `ThreadedScheduler` adds the pool.
- **Delete `ThreadPolicyType`** and pass a plain thread-name string. In this
  revision `thread.cc`'s `setup_thread`/`run_thread` *ignore* the policy entirely
  — they only call `pthread_setname_np("Xapiand:" + name)` (`thread.cc:71-88`),
  with no affinity or priority — so the elaborate per-layer policy plumbing has
  no runtime effect today and can collapse away.
- Replace `threadpool.hh`'s use of `strings::format` (for the worker name) with
  `fmt`/`std::format`.

**Difficulty: Moderate. Value: High** (a cancellable, O(1)-ish timer wheel that
scales to many short-horizon timers is broadly useful).

### 3. `debouncer` — depends on the scheduler

`Debouncer` + the factory functions. To extract:

- Vendor `callable_traits.hh` (already a standalone reuse candidate) and the
  three `random` helpers (or template the RNG).
- Stub the `L_*` macros.

**Difficulty: Easy** once the scheduler is out. **Value: High** — the randomized
force-window debounce is a genuinely useful, hard-to-find building block.

### Shared coupling to cut (all tiers)

| Coupling | How to cut |
|---|---|
| `log.h` / `colors.h` macros | No-op stubs, or one injectable trace callback. They already compile to `L_NOTHING` by default, so this is mechanical. |
| `ThreadPolicyType` (`thread.hh`) | Delete — runtime-ignored; replace with a name string. |
| `Thread` / `ThreadPool` (`thread.hh`, `threadpool.hh`) | Bundle trimmed copies, or target `std::jthread` + a minimal pool. Both are already flagged as independent reuse candidates. |
| `callable_traits.hh`, `random.hh` | Vendor (both standalone). |
| `strings::format` (threadpool worker name) | `fmt` / `std::format`. |

### Suggested packaging

Two reasonable shapes:

- **Three repos** — `stash`, `timer-scheduler` (deps: stash + a thread/pool lib),
  `debouncer` (deps: timer-scheduler + callable_traits). Mirrors the existing
  base-x / uinteger_t / fantasyname precedent.
- **One `scheduler` repo** bundling all three, with `stash` as a vendored or
  submoduled dependency, plus `callable_traits` and the thread pool as their own
  tiny libs (the pool is independently worth publishing).

Either way `stash` should be its own thing: it's the most novel and the most
broadly reusable, and it carries the fewest dependencies.
