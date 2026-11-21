# group-heal--install: journal

State: working
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1, strata worktree strata-wt/xapiand/group-heal--install (base strata:51dcff1e): the install
transfer engine, transport-agnostic, in `include/strata/group/install.hh`, with its tests in
`test/group/install_test.cc`. Leader: pin (strata checkpoint generation + file list with
CRC32C), chunk server (64 KiB). Learner: plan (lacks / holds short), staged fetch under `is.`
(strata) and `ia.` (file set) names, `ip` marker, activation (renames, strata install, sweep),
and recovery's completion of an interrupted activation.

## Next

1. Engine + tests (values, crash at every step, mutant skip-file-set).
2. Blocked items below for the Driver binding (A1's promote/replace/remove, A2's schedules).

## Blocked on

See "Findings for the lead" (filled as found).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |

## Result

<pending>

## Rulings I made

- consensus pin stays at b8de3b8 (consensus-quiescence, which group-core needs for
  closed_state/reopen); its snapshot headers (snapshot_channel.h, snapshot_transfer.h,
  snapshot.h, membership.h) are byte-identical to consensus main 8801b99, so "build on main"
  holds for the install surface. CMakeLists.txt is not in this task's files either.
