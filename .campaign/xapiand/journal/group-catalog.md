# group-catalog: journal

State: working
Quest: .campaign/xapiand/quests/group-catalog.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code round done (group.hh, catalog.hh, test/group/group_catalog_test.cc, the migrated tests, the
note); building and running the group tests locally before the jobs.

## Next

1. Targeted local run of group_* and catalog tests green; commit work, then the test migration
   separately.
2. Mutant commits on side branches: A2 `close-skips-checkpoint` (close()'s `if (!fenced_.load())`
   -> `if (false)`), A3 `close-in-commit-hook` (the test's commit hook closes the group,
   close_payload does nothing, blocking()'s lease guard removed): `--fails-without` jobs.
3. `campaign verify group-catalog --for A1,A2,A3`; push the branch for asan/tsan CI.

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
| A1-A4 | none yet |

## Result

<When ready for review.>

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
