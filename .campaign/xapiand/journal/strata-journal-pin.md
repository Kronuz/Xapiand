# strata-journal-pin: journal

State: ready for review: the durability round's fixes (F1 strict bound, F2 witness test) are confirmed at strata:7214f27d; A1 (build-test, held by slow-once until now), A2 261123-235932-f57dc8 and CI run at the round's end (lead, 2026-11-24)
Quest: .campaign/xapiand/quests/strata-journal-pin.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Branch at strata:bb2f8a5a: review CHANGES (strata-journal-pin-durability.md) folded in. F1: a head at the
bound holds that record's whole batch, so attach_log loses only strictly below the bound (`<`); t5b keeps
the group at the bound and at bound + 1. F2: lifetime case bounded-witness. Jobs queued at bb2f8a5a.

## Next

1. Read the jobs; a failure is the next fix. A1 builds with asserts on (-O3, no NDEBUG): the
   loop-ownership assert that failed CI at strata:2b530925 is covered (turn_seal.hh already adopts
   the loop, from turn-log-parity).
2. Push once only jobs remain; watch CI for A3.

## Blocked on

Nothing. A damaged replica's path back (a lost or bounded group cannot be opened again) is filed in
hunt.md as its own follow-up, not this quest.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test (asserts on) at strata:7214f27d: `campaign verify` at the round's end |
| A2 | 261123-235932-f57dc8: group_tests, `ctest -R ^(group_|simulation_group_)` at strata:7214f27d |
| A3 | CI at the landed commit |
| A4 | `docs/mechanisms/replication-group.md`: "opens a bounded group through" (section "A damaged turn log"); Result below |
| A5 | standalone_turn_log t5 t5b 261123-235930-a76b46; fails without: bounded-as-stored 261123-194337-4c3177, 261123-235110-d0d366; bounded-at-bound (`<` to `<=`) 261124-000525-fb418d; earlier 261123-194339-539e7b |
| A6 | group_lifetime_test bounded + bounded-witness 261123-235934-e3aa7b; fails without: bounded-created 261123-194341-31812e, 261124-000527-82a382; bounded-witness-created 261124-000529-7f92f7 |

Review findings (strata-journal-pin-durability.md):
- F1 (blocking): fix strata:bb2f8a5a; test job 261123-235930-a76b46 (at-bound and bound + 1 cases kept, below lost);
  mutant 261124-000525-fb418d (`<` to `<=` must fail t5b); the reviewer's probe at_bound.sh run at the
  pre-fix strata:1cbb6e30, where it patches `<=` to `<` (it cannot patch the fixed commit): 261123-232624-4b30b6.
- F2: witness route test 261123-235934-e3aa7b (bounded-witness), mutant 261124-000529-7f92f7.
- F3 hunt.md; F4 lost still goes to create (one-word follow-up); F5 index-delete's.

## Result

A4, journal 9e11713 -> 9c7e31c (pin strata:5d30f788, merge of main over strata:2b530925), what strata follows:
1. `Host::delete_group` (journal:131eb2e), `GroupState::deleted`: new, reported `none` by status; the
   pin carries it, index-delete calls it. Pin: strata:2b530925, strata:5d30f788.
2. `GroupStatus::bounded`, `Host::bound`/`lose` (lost bound, docs/mechanisms/lost-bound.md): a damaged
   turn log keeps the group with its records after the bound. strata follows it twice:
   - the index (strata:8f002718, corrected at strata:bb2f8a5a): attach_log continues a bounded group when
     the head holds it through the bound, else `lose(g)` and log_lost; a head AT the bound holds that
     record's whole batch (a commit seals it), so only a head strictly below loses (turn-log-parity's `<`);
     test t5b (below lost; at the bound and bound + 1 kept), mutants.
   - the replicated group (strata:22ee0282): group and witness open a bounded group through
     `driver.recover`: Journal's Store loses it durably and throws Corruption; `driver.create` threw
     logic_error and left it bounded.
3. `create_group`/`reopen_group` commit with the loop's next turn with records, not on return: strata's
   join order holds; turn_seal.hh's comment says so (strata:8f002718).
4. Table shards renamed `groups-<slot>-<node>`, old files unread: strata never names them; an old host
   directory does not reopen (formats open until release).
5. `TurnDriver::park/parked` -> `evict/evicted`, `Hold` takes a required loop, `durable()` refuses a
   scheduler without `post_call`, `framing::Manifest::groups` a `Groups`: strata calls none of them
   (git grep); consensus's driver calls `durable()` (A1 runs it).
6. Loop ownership (journal host-ranges.md): group_io and table commits run on the group's loop;
   strata's turn_sealer adopts it under its turn mutex (turn-log-parity, strata:f5f6b7dc). Before it the
   asserts failed CI at strata:2b530925.
7. Between a3eecb5 and 9c7e31c (journal main after detent's quests): Host::delete_group, memory_io.h
   removed (group_io_bench runs on faultfs), the cut-header design and earlier-turn damage reporting
   (journal:921301d): damage before a later write of its block is reported, which only adds bounded or
   lost outcomes; nothing in strata's API changed. detent:journal-faultlab-repin had not moved the
   contract at 9c7e31c (faultlab pin only).

## Rulings I made

1. A2: no `simulation_group_*` rows exist at the old pins; the closest are the `group_*` ctest rows, run
   with the simulation_group_ rows when they exist (the job's regex takes both).
2. t5b damages one block with the head below, at, and past the bound: a head past needs no checkpoint
   record after the damage (it would make the group whole), so the commit's checkpoint turn is withheld
   by powering the disk off before it. The head at the bound is the write that straddles the damaged
   block into the next (its block's manifest then names it); bound + 1 is the write after it.
3. `lost` still goes to `create` in group.hh: it throws as before; only `bounded` moved (the lead's
   scope). A lost group's open is a clearer Corruption through recover too: a one-word follow-up, not
   taken here.
4. The mutants run in a dedicated slot (sjp-mutants), chained, so a mutated source never reaches
   another job's build in the quest's slot.

## Lead rulings (2026-11-23)

- The `bounded` handling exists once, on turn-log-parity's branch (index.hh attach_log: a bounded group whose
  head is below the bound is lost with log_lost; accepted under its durability review). This quest lands after
  turn-log-parity, then takes main and rebuilds its pin on top: no second copy of that check here.
- group.hh:214 (a bounded group goes to driver.create) is group-heal's: filed in hunt.md for its durability re-review.
- F10 of group-heal's durability re-review (2026-11-23): on journal main a bounded group reaches driver.create in group.hh, which throws; the replica never rejoins. This quest takes it after group-heal lands (group-heal holds group.hh).
- CI at strata:2b530925 (lead, 2026-11-23): every platform fails standalone_turn_log and kv_strata_batch on journal main's
  loop-ownership assert ("group_io and the table commits run on the group's loop; adopt_loop hands it over",
  owned_ranges). The build-test job passed only because Release builds compile asserts out. The bump must adopt
  journal's loop handoff: strata's turn sealer calls adopt_loop, and the t6w workaround (strata:6421d4b3) goes
  (hunt.md, "Drop group-core's t6w workaround"). turn_seal.hh is turn-log-parity's, so this lands after it, as ruled.
  The build-test check must run with asserts on, or CI is the only proof; the next contributor proposes how.
