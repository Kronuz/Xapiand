# x-pool: journal

State: working
Quest: .campaign/xapiand/quests/x-pool.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Root cause of the docs E2E failure at Xapiand:0638e61fd. Local tools in the worktree's `.scratch/`
(untracked): `build.sh plain|asan|tsan` (ccache, `.scratch/build-<kind>`), `probe.sh` (one RESTORE
alone), `probe2.sh` (the E2E's twitter writes, then the RESTORE), `e2e.sh BIN TAG PORT`.

Findings so far:
- The RESTORE of the 1,000 accounts is not stuck but slow in the E2E: 29.3 s under ASan (passed by
  0.7 s), over the 30 s newman timeout plain and under TSan (the node logs "took ~30852 ms"); the
  `cities` RESTORE 18 to 24 s. Step 2 (`Xapiand-wt/x-pool/.scratch/e2e*`): 1.8 s plain, 4.6 s ASan,
  13 s TSan. Alone, on a fresh node, the RESTORE takes 3.6 s on step 3's build, step 2's and a
  pre-pool build alike (load ~11 of 16 cores).

## Next

1. Reproduce in the E2E's context (probe2, e2e.sh on step 3's build), sample the node, find the cause.
2. Fix with a regression test that fails without it (jobs at both commits); A17 e2e plain, A6 TSan.
3. Verify the last four commits (build-test); then A8, A10 to A12, A14 to A16; A9, A13 in the design;
   A18 the note; step 4 (A5).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|

## Result

## Rulings I made
