# group-catalog: journal

State: waiting on jobs 261121-103856-a3faa5 261121-103905-bc298a 261121-103914-1a21ad
Quest: .campaign/xapiand/quests/group-catalog.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done at strata:096dbd04 (pushed; CI running). Waiting on the A2 and A3 mutant jobs and the
suite.

## Next

1. When 261121-103856-a3faa5 (A2 mutant), 261121-103905-bc298a (A3 mutant) and
   261121-103914-1a21ad (suite) pass and CI (asan, tsan) is green: paste `campaign brief`'s
   table below, State "ready for review". A failed mutant job means the test misses it: read its
   log (`campaign job log ID`) before changing anything.

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
catalog's ownership follow[s]" (not in my file set); mutants.py's `destroy-keeps-member` targets a
destructor text main no longer has (pre-existing at main, untouched).

## Rulings I made

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
