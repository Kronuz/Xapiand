# strata-journal-pin: journal

State: waiting on jobs 261123-082128-ac3d19 261123-082130-ef040a; blocked on lead: file set for the bounded-group adaptation (Blocked on 1, 2)
Quest: .campaign/xapiand/quests/strata-journal-pin.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pin bumped (strata:2b530925, journal 9e11713 -> 9e0649d, 80 commits, contains journal:131eb2e).
A1 build-test 261123-082128-ac3d19 and A2 group rows 261123-082130-ef040a queued at strata:2b530925.

## Next

1. Read both jobs: a compile or test failure from the API diff is the next fix (all in the file set
   unless it names index.hh, turn_seal.hh or group/**).
2. On the lead's ruling on Blocked on 1: add `bound`/`lose` to turn_sealer, handle `bounded` in
   index.hh attach_log, a damage test in standalone_turn_log_test.cc that opens silently without it.

## Blocked on

1. Lead: journal's lost bound (2026-11-21) adds `GroupStatus::bounded`. A damaged turn log no
   longer loses a group: Host::open keeps it with its records after the bound
   (docs/mechanisms/lost-bound.md names strata's index as the reader that continues it).
   strata's `index::attach_log` (include/strata/core/index.hh:3364) handles lost/none/closed and
   treats `bounded` as stored: it replays the records after the bound with no gap check, so an
   index whose head is below the bound opens silently missing the damaged batches. Before the
   bump that group was `lost` and the open threw log_lost. Fix (outside the file set:
   include/strata/core/index.hh, include/strata/core/turn_seal.hh, test/standalone_turn_log_test.cc):
   turn_sealer gains `bound(g)` and `lose(g)` over Host's; attach_log, on `bounded`, continues
   only when `cur_.applied.index > bound` (a head at {bound, i} may lack the batch's later
   commands), else `lose(g)` and throws log_lost. Recommendation: widen the file set to those
   three files and resume me; the alternative (land the pin now, fix in a follow-up) leaves main
   with a silent-loss path on log damage. Also there: turn_seal.hh's "Each is durable when it
   returns" for create/reopen is stale (journal now commits transitions before the loop's next
   turn with records; strata's join order still holds, the first record's turn commits it first).
2. Lead / group-heal: include/strata/group/group.hh:214 recovers only `stored`/`closed`; a
   `bounded` group falls to `driver.create` over an open table entry (unverified what consensus does then). group-heal holds
   the file; the fix is to route `bounded` to `driver.recover` (consensus's Journal Store loses a
   bounded group on recover, per journal's changelog) or to `lose` it first. Not touched.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261123-082128-ac3d19 at strata:2b530925 (queued) |
| A2 | 261123-082130-ef040a: `ctest -R ^group_` at strata:2b530925 (queued) |
| A3 | CI at the landed commit |
| A4 | Result below |

## Result

A4, journal 9e11713 -> 9e0649d, what strata follows:
1. `Host::delete_group` (journal:131eb2e), `GroupState::deleted`: new, reported `none` by status;
   index-delete calls it. Pin: strata:2b530925.
2. `GroupStatus::bounded`, `Host::bound`/`lose` (lost bound): behavior change strata must follow,
   Blocked on 1 and 2; no strata commit yet.
3. `create_group`/`reopen_group` commit with the loop's next turn, not on return: strata's join
   order holds; turn_seal.hh comment stale (Blocked on 1).
4. Table shards renamed `groups-<slot>-<node>`, old files unread: strata never names them; an old
   host directory does not reopen (formats open until release).
5. `TurnDriver::park/parked` -> `evict/evicted`, `Hold` takes a required loop, `durable()` refuses
   a scheduler without `post_call`, `framing::Manifest::groups` a `Groups`: strata calls none of
   them (git grep); consensus's driver calls `durable()` (A1 runs it).

## Rulings I made

1. A2: no `simulation_group_*` rows or test/group/sim_suite.sh exist at strata:93d10446; the
   closest are the `group_*` ctest rows (voters, lone voter, lifetime, catalog, footprint, file
   set), run as A2.

## Lead rulings (2026-11-23)

- The `bounded` handling exists once, on turn-log-parity's branch (index.hh attach_log: a bounded group whose
  head is below the bound is lost with log_lost; accepted under its durability review). This quest lands after
  turn-log-parity, then takes main and rebuilds its pin on top: no second copy of that check here.
- group.hh:214 (a bounded group goes to driver.create) is group-heal's: filed in hunt.md for its durability re-review.
