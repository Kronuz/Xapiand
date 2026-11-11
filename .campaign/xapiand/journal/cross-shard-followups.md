# cross-shard-followups: journal

State: waiting on jobs 261111-133750-2603c2 261111-133753-938445 261111-133756-65f3d0 261111-133814-8d444b
Quest: .campaign/xapiand/quests/cross-shard-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

N1-N3 are committed (Xapiand:d291dbe03) and pushed. Jobs (the lead's inline resubmission; my first three, which ran a
script outside the repo, were cancelled): 261111-133750-2603c2 (A1/A3 pass), 261111-133753-938445 and
261111-133756-65f3d0 (A1 fails-without, must_exist disabled by sed). Suite for A4: 261111-133814-8d444b.
Every job's command goes inline or in a committed file from now on.

## Next

1. When the four jobs finish, read results and CI.
2. Paste `campaign brief`'s Evidence table below, fill Result, set State "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261111-133750-2603c2 (pass), 261111-133753-938445, 261111-133756-65f3d0 (fails-without); pending |
| A2 | reasoning below (no test can reach it) |
| A3 | src/engine/strata/shard.cc: `assert(!args.must_exist || !native.key().empty())` |
| A4 | 261111-133814-8d444b; pending |

## Result

N1: the replace_by_term and PATCH tests (tests/test_node.cc) run their tasks on `ParkHost`, a one-thread
engine::Host that reports `idle()`. A task that stops with the host idle is parked on the held turn (its
take_turn armed its deadline and queued its lock in one step of that thread), and the next task starts only then,
so the queue order lookup/read, erase, write is by construction; the test asserts each is not done at its step.
The PATCH test calls `DatabaseHandler::patch` directly (the HTTP layer adds nothing to the race) and keeps the
GET checks. No sleeps order either race. The other test of that quest (stale write, HTTP) is untouched.

N2: `index(did)` catches `errc::doc_not_found` only. A test cannot reach the case: a `not_found` after an applied
write comes from `finish_write`'s commit, and strata raises not_found only on open ("No such database"), which a
shard that took the write has passed; no fault hook exists to make a commit fail with not_found (adding one is a
new mechanism). Reasoning only, as the quest allows.

N3: a must_exist write asserts its document has a key (src/engine/strata/shard.cc); only built with ASSERTS.

N4: stays known: a failed must_exist write after its blob push leaves those blobs unreferenced in the found
shard's storage; the storage's sweep is where it belongs.

## Rulings I made

- N1 mechanism: a test-local host with an idle signal, not a queue-depth counter on ShardEndpoint/async_mutex
  (that needs pool.h and the task library, outside the file set). Idle after a task starts means parked only
  while nothing it awaits resumes from another thread; the tests' later checks (the tasks complete after the
  release, and the end state) would hang or fail if that were wrong.
- Mutants are applied by sed inside the job's own command (the lead's resubmission).
