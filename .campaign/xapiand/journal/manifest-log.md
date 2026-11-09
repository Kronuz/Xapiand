# manifest-log: journal

State: working
Quest: .campaign/xapiand/quests/manifest-log.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 3a, the log layer (strata `include/strata/core/manifest_log.hh`, nothing in `index.hh` yet):
the design text for C4 to C6 and D41; `apply()` in proportion to the edit; the log reader (D6.3's
caller rules, D7's bounds on read); the log writer (one write per append, the truncate-back, one
record in flight); crash-image and bit-flip tests over the reader.

## Next

1. Step 3a as above, with its tests and mutants, as jobs.
2. Step 3b: `index.hh` on the log (`m_` becomes the log's state, `commit_files` appends or rolls
   over, `recover` replays and issues the open's one `sync_data`, `retain` by D8, `check()`, the
   live-docs, metadata and field files retired, the allowance, `pre_freeze`).
3. Step 3c: T4's cases on faultfs, T6's oracle through the index, `media_test`; A9's integration
   jobs (prospect needs a companion change, Blocked on).

## Blocked on

<filled as found>

## Evidence

| Criterion | Evidence |
|---|---|

## Result

<When ready for review.>

## Rulings I made

- R1 (scope): step 3 does not fit one review: `index.hh`'s commit, recovery, retention, `check()`,
  install and the retired files touch about 160 lines that name the manifest, live-docs, metadata
  and field files, plus about 90 test sites in strata and prospect's tests that read the old files.
  I split it into 3a (the log layer, reviewable alone), 3b (the index on the log) and 3c (the crash
  suite and integration), each its own commits, as the lead's prompt allows.
- R2: merged strata `main` (strata:3df7af4) into the branch rather than rebasing, so the
  reviewed design commits (strata:1c8ff50, strata:79beaf4) the reviews cite stay reachable.
