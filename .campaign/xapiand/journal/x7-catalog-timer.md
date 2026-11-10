# x7-catalog-timer: journal

State: waiting on jobs 261110-143638-823338 261110-143640-b9f269 261110-143641-8f8cf6 261110-143654-d857c1
Quest: .campaign/xapiand/quests/x7-catalog-timer.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

D58 (option a) implemented at Xapiand:c0d0208ee; jobs running.

## Next

1. Read the four jobs; fix what fails; then State "ready for review".

## Blocked on

1. **File set** (lead): the constructor now takes the control loop (`DatabasePool(engine::AsioHost& timers, ...)`),
   so every test that builds a pool passes one. That touches, outside the set: `tests/pool_support.h` (the shared
   `control_loop()` test host), `tests/test_reactor_requests.cc` (10 pools), `tests/test_blocking_guard.cc` (4),
   `tests/test_wide_search.cc` (1); each pool line gains `control_loop(), ` and an include. Ask: add them to the
   set. The alternative, a constructor without a timer host, is a second timer mechanism.

Answered: the lead's D58 message (no msg id): merged master (Xapiand:5b57ca685), implemented (a) as below.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | t1_census at the head 261110-143640-b9f269 (pending); at master 66b310e60 261110-143641-8f8cf6 (pending, must fail). Both copy `harness/gate7c_rows.sh` and `harness/stall_load.py` from x7-gate's e7e8e03c5 (`--ref GATE`), since gate7c_rows.sh is not on master |
| A2 | build-test 261110-143638-823338 (pending; test_pool's sweep, TTL and erase cases); mutant `refs/mutants/x7-catalog-timer/no-arm` (21839dcc6, `after()` returns before arming) 261110-143654-d857c1 (pending, passes only when `xapiand_test_pool sweep` fails) |
| A3 | `docs/mechanisms/index-registry-on-catalog.md` item 10: "The timer the catalog needs runs on the control loop's timers: each callback is an Asio timer armed on the loop's thread with `engine::Host::start_timer`" |

## Result

`DatabasePool::Timer` (src/database/pool.cc) arms each catalog callback as an Asio timer on the control loop via
`Host::start_timer`, posted to the loop's raw executor (which runs after the task scheduler closes); it keeps each
callback until it fires, and `close()` (in `~DatabasePool`, after the shutdown, before the catalog) runs the rest at
once. The DB-TIMER thread and the scheduler include are gone. Manager: `_rx` is made before the pool and outlives it;
`run_control()` runs the stopped loop on the main thread, used by `drain_control`, the 50 ms poll of Stop 1's wait and
each 10 ms slice of Stop 3's wait. Note: docs/mechanisms/index-registry-on-catalog.md item 10.

## Rulings I made

1. One Asio timer per catalog callback, not a queue under one timer: the catalog arms few (1 Hz sweep, busy-lease
   reposts), and the pool keeps the callbacks so close() can run them.
2. Arming hops to the loop's thread (AsioHost arms and cancels there only), with `asio::post` to the raw executor
   rather than `post_call`, which drops calls once the loop's scheduler closes before Stop 3.
3. After close(), a timer runs its callback at once (the catalog's destructor may arm while it waits; a callback only
   posts).
4. Stop 1 runs the control loop at its 50 ms poll, so the autocommits that come due then run, where before they waited
   for Stop 2. Stop 3's slice drops from 500 ms to 10 ms (timer latency there).
5. Pool reset order: the pool is reset at manager.cc's end of join(), long after the loop stopped running (break_loop,
   before join) but before `_rx` is destroyed; the loop "running" through the stop is the main thread's pumping above.
