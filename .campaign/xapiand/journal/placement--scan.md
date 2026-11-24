# placement--scan: journal

State: waiting on jobs 261121-131722-ace692 261120-181211-11c0e5 261120-181214-13639d
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Scan at strata:2620ba75 (include/strata/placement/scan.hh, test/placement/scan_{glob.hh,test.cc,mutants.py,cmake}),
origin/quest/xapiand/placement merged in (lead's strata::placement target, test/placement/*.cmake glob);
CMakeLists.txt change dropped. Pushed. Old jobs at 5494733b cancelled. Jobs queued:
- 261121-131722-ace692 (A2): build placement_scan_test, run cases + model + bound (20000 seeds).
- 261120-181211-11c0e5 (A2, --fails-without): `bound` mode at mutant no-seek-past (leave() reads
  on with next() instead of seek/skip); it must fail.
- 261120-181214-13639d (A2): the whole build-test suite on the task branch.

## Next

1. Read the three jobs. A compile or test failure in the first: fix, commit, resubmit all three.
2. Once green: paste `campaign brief`'s A2 row in Evidence; send root the scan's section text for
   docs/mechanisms/placement-root.md ("The seekable scan"; scan.hh cites it).

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A2 | jobs 261121-131722-ace692, 261120-181211-11c0e5 (mutant), 261120-181214-13639d: pending |
| msg-1 | Mine (scan -> root, key order). The lead made path order a decision (quest file, Xapiand:8ff0a0f5f: '/' stored as byte 0); root answers it. |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Key order (msg-1, sent to root): the scan assumes PATH ORDER, segment by segment (twitter <
  twitter/user < twitter-a), i.e. '/' encoded as a byte below every name byte (0x00; names hold no
  byte < 0x20). Raw '/' bytes interleave a subtree with its siblings and break the seek-past bound.
  The scan reaches records only through a source (seek, skip past a subtree, next), so root's
  layout plugs in by an adapter.
- Matcher contract: a segment automaton (start, step, accepts, live) plus skip_to(state, after),
  the least segment > after that may match ("after" when unknown, nullopt when none). Xapiand's
  Glob needs skip_to added: each live non-`**` position's literal prefix bounds the segments it can
  match (ported in test/placement/scan_glob.hh).
- The reference glob matcher lives in the test (a port of Xapiand's MIT index_pattern::Glob, with
  its item rules: `-p`, `_all`, `.` segments at any depth never listed); the brute-force model is a
  separate recursive matcher, so the automaton is checked too.
- Test registration lives in test/placement/scan.cmake (lead's ruling: CMakeLists.txt is the lead's).
- The mechanism note belongs in docs/mechanisms/placement-root.md (root's file); scan.hh points to
  its section "The seekable scan". I send root the text when the scan is green.
