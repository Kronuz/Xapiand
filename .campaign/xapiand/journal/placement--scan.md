# placement--scan: journal

State: working
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Scan at strata:5494733b (include/strata/placement/scan.hh, test/placement/scan_{glob.hh,test.cc,mutants.py}).
First job 261120-175303-728838 builds and runs placement_scan_test (cases, model, bound).

## Next

1. Fix whatever the first job shows; then the A2 jobs: the full suite gate, `bound` at the
   no-seek-past mutant with --fails-without.
2. Push quest/xapiand/placement--scan.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

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
- CMakeLists.txt is outside the quest's file set; I added one 4-line block registering
  placement_scan_test (the suite must run it). Lead: widen the set or take the block.
- The mechanism note belongs in docs/mechanisms/placement-root.md (root's file); scan.hh points to
  its section "The seekable scan". I send root the text when the scan is green.
