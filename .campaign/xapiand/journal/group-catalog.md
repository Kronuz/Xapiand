# group-catalog: journal

State: waiting on jobs 261121-135825-03ec6d 261121-135851-757e6d 261121-135852-69c5e6 261121-135854-c3ed31 261121-135856-0ef165 261121-135858-646f53 261121-135859-aa398e 261121-135901-443c9b 261121-135903-e4b096 261121-135915-f674a1 261121-135916-f7ee81 261121-135918-6dc4d8 261121-135920-9c9fde 261121-135932-46de17 261121-135942-dacf70 261121-132654-daff1f
Quest: .campaign/xapiand/quests/group-catalog.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Durability review (CHANGES at strata:096dbd04, `.campaign/xapiand/reviews/group-catalog-durability.md`),
fix round 1 at strata:add2f00a (pushed; CI running). Code never compiled locally: the first job
(261121-135825-03ec6d) is also the compile check; every other job runs `--after` it.

## Next

1. When 261121-135825-03ec6d (every group_catalog case + lifetime failed-open) passes, read the
   mutant jobs (each must exit nonzero: the test kills it), the TSan job 261121-135932-46de17 and
   the suite 261121-135942-dacf70. A compile error or a failing case: `campaign job log ID`, fix,
   resubmit the chain (round 2 is the last).
2. Read the reviewer's probe 261121-132654-daff1f (at 096dbd04) when it lands: a TSan report in
   `attach` or `failed-open` there is the pre-fix race, the R1/R2 attribution.
3. All green and CI green: Evidence table below, State "ready for review" with
   `campaign accept group-catalog durability --confirm R1=<tsan> R2=<tsan> R3=<suite> R4=<suite>`
   for the lead.

## Blocked on

Nothing.

## Messages

- msg-201 (mine to the lead, answered by the lead): (1) file set widened to the four group tests
  and mutants.py; their migration is its own commit (construction sites, the footprint's host-side
  index close, the two obsolete A6 mutants). (2) Option (a) taken: the commit hook stays a strata
  commit; `close_payload` runs the group's close outside the write lease; A3 revised by the lead.

## Functions changed (for the lead's merge with group-heal--install)

group.hh: `config` (uuid and strata removed), new static `open_index`, the constructor (borrowed
`jn::IO&` and `index_type&`), `~group` (detaches only while attached), `close()` (rewritten:
teardown on the loop via new `tear_down()`, `chore_idle()`/`idle_awaiter`, `torn_down()`), new
`open_blocking`/`close_blocking`/`blocking`, `drain()` (posts idle waiters when it stops),
`recover()` (no index open), members `ix_` (now `index_type*`), `dir_` removed, `attached_`,
`idle_` added. catalog.hh: `opening::io`, `host::close_payload`, `close_payload(slot&, bool)`,
`run_open` (sets op.io), `run_close` and `run_replace` (call it before `index.close()`),
`stats_t::payload_close_failures`.
Durability round: group.hh `group(...)` (no attach), `open()` (attaches on the loop; failure
path calls `leave()`), new `leave()` (shared by close), `blocking()` (inline-scheduler guard),
`attached_` (false until open). catalog.hh unchanged.

## Durability review findings (round 1)

| ID | Fix | Test (fails without) | Jobs |
|---|---|---|---|
| R1 | strata:c542849f: a failed open() fences, closes its row if open, then `leave()` (detach on the loop, refuse items, wait for the chore to go idle) before it throws; close() shares `leave()` | group_catalog `failed-open` (open_payload checks the loop's table after the throw; another group writes meanwhile; two failed pins) and lifetime `failed-open` (table checked after the failure); TSan over both | run 261121-135825-03ec6d; mutant failed-open-stays: 261121-135851-757e6d (catalog), 261121-135852-69c5e6 (lifetime); TSan 261121-135932-46de17 |
| R2 | strata:c542849f: the constructor no longer attaches; open() attaches on the loop before its Driver call (a duplicate id refused, nothing begun) | group_catalog `attach` (five groups open on the lane while one writes; open_payload checks the table right after construction) and lifetime `failed-open` (table checked after construction) | run 261121-135825-03ec6d; mutant attach-at-construction: 261121-135854-c3ed31 (catalog), 261121-135856-0ef165 (lifetime); TSan 261121-135932-46de17 |
| R3 | strata:0d9e90a7 (tests only) | group_catalog `sweep` (idle eviction, bundle at everything applied, reopen recovers it), `bound` (demoted slot, eight wakes failed by EMFILE on segment opens, bound close, reopen), `replace` (index fenced by a failed sync under the host's commit, sweep replaces it, close_payload ran, reopen replays from the log) | mutants sweep-skips-payload 261121-135858-646f53, bound-skips-payload 261121-135859-aa398e, replace-skips-payload 261121-135901-443c9b |
| R4 | strata:0d9e90a7 (tests only) | group_catalog `bare-commit-crash`: bare commit over unsynced file effects, power off, shutdown names the key, crash with 16 seeds, P_H 5 durable, reopen at P_b 0 reads every value | mutant replay-from-ph (effects skipped at or below P_H) 261121-135903-e4b096 |

Suite: 261121-135942-dacf70. Reviewer's probe at 096dbd04: 261121-132654-daff1f (queued).

Non-blocking notes:

- N1 (refused close after close_payload leaves a dead group in an open slot): not fixed. The end
  at the prior state is the catalog's I3-N6 contract, asserted by `catalog_policy_test`
  `refused_close_throws` (outside my file set), and host.close_payload's doc already says a
  refused close calls it again. For the lead: once close_payload ran, retry the close (as
  shutdown does, slot kept `closing`) instead of `close_refused`; needs that test changed.
- N2 (blocking-guard refusal then destroyed live group): not fixed in code; with R2 a refused
  open_blocking leaves an unattached group, safe to destroy. A refused close_blocking stays a host
  bug (the catalog calls it on its lane outside the lease); documented in group-catalog.md "The
  hooks". Making it fatal would break A3's refusal check (logic_error).
- N3 (inline-pumped schedulers): fixed in strata:c542849f, the guard also compares
  `task::current()` with the loop's and the chore's scheduler. No test (the catalog never pumps
  inline); reviewer's case.
- N4 (chore writes with no pin): documented in group-catalog.md "The hooks" (CT-R15 counts the
  calls; a demoted slot's apply promotes itself outside the wake accounting). An exemption in the
  check is the catalog's design; left for the lead.
- N5 (sweep never evicts a group with uncommitted applies): the owner's, forwarded under Open for
  the lead below.
- N6: mutants.py retargeted (strata:0d9e90a7): `destroy-keeps-member` becomes
  `close-keeps-member` (close's detach; footprint) 261121-135915-f674a1; `close-keeps-waiters`
  261121-135916-f7ee81 and `no-closed-mark` 261121-135918-6dc4d8 had gone stale in this quest's
  round 1 (513d959d), `failed-open-keeps-row` in this round 261121-135920-9c9fde.
  replication-group.md's status line is outside my file set: still the lead's.

CI at 096dbd04: every job green but macos-arm64, whose `manifest_log` failed ("deleted sets");
this quest touches no manifest-log code and main's last six runs are green but one. Read the
add2f00a run before calling it pre-existing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-103852-b1cb8c (group_catalog_test at strata:096dbd04, exit 0) |
| A2 | 261121-103852-b1cb8c; mutant 261121-103856-a3faa5 at strata:053aede3 (branch mutant/group-catalog/close-skips-checkpoint, local) pending |
| A3 | 261121-103852-b1cb8c; mutant 261121-103905-bc298a at strata:1d3155e7 (branch mutant/group-catalog/close-in-commit-hook, local: commit hook closes the group, close_payload empty, lease guard removed) pending; TSan is CI's |
| A4 | [artifact] strata `docs/mechanisms/group-catalog.md` (strata:513d959d) |

Suite: 261121-103914-1a21ad pending. Local targeted runs (not evidence): all 9 group_* tests and
`catalog` green at strata:096dbd04.

## Result

strata:513d959d lends a group its slot's index and directory (constructor references,
`group::open_index` for the host), makes close() tear down on the loop and return with the chore
idle, adds `open_blocking`/`close_blocking` with self-wait guards, and gives the catalog
`opening::io` and `host::close_payload` (run before every `index.close()`, outside the lease).
strata:096dbd04 migrates the group tests. How it works: strata `docs/mechanisms/group-catalog.md`.
Open for the lead: strata `docs/mechanisms/replication-group.md`'s status line still says "the
catalog's ownership follow[s]" (not in my file set); mutants.py's stale mutants are retargeted (N6).
Open for the lead (durability review): N1 (catalog contract, catalog_policy_test), N4's check
exemption, N5 (whether the sweep commits a group's uncommitted applies: the owner's).

## Rulings I made

7. (Durability round) A failed open sets `closing_` and tears down fully, so a later close()
   returns at once rather than throwing `group_fenced`; no test relied on the throw.
8. R1's and R2's deterministic tests check the loop's table on the loop from open_payload (and
   lifetime_test's loop thread); TSan carries the race itself. The checks are counted in the
   process and asserted at its end, on the main thread.
9. group_catalog_test takes a case name; ctest runs `borrow` as `group_catalog` (A1-A3, as before)
   and each new case as `group_catalog_<case>`. The harness's directory wrapper fails segment
   opens with a host errno (EMFILE): an EIO would mark the segment damaged instead of failing the
   wake.

1. The borrowed index and directory are constructor references, not `config` fields: references
   cannot be null, and `config` is freed after open while the borrow lasts the group's life.
   `config` loses `uuid` and `strata`; the host opens the index with `group::open_index`.
2. The slot's directory reaches `open_payload` as `catalog::opening::io` (a new field), so the
   `open_payload` signature Xapiand assigns today is unchanged.
3. `close_payload`'s throw is counted, names the key at shutdown and the index closes anyway (H is
   done with it), rather than refusing and retrying the close: a fenced group would otherwise
   retry forever and stall shutdown.
4. `close()` always tears down (fenced or failed too) and waits for the chore to go idle before it
   returns, and detaches from the loop on the loop: the catalog destroys H on its lane right after,
   which previously raced the chore's last `drain()` steps and the loop's member table.
5. `close_blocking`/`open_blocking` refuse on the loop, on the chore, or under the index's write
   lease (logic_error, nothing started): explicit failure over a silent self-wait.
6. The two A6 mutants `close-keeps-index`/`close-keeps-directory` are deleted: the group no longer
   closes either; the footprint test's host closes the index after the group (borrow helper
   `close_index`), so its "a closed group holds no descriptor" check stands.
