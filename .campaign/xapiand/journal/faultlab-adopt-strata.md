# faultlab-adopt-strata: journal

State: waiting on jobs 261121-050042-88219e (A1 suite at strata:efa5214b) 261121-050043-dd30d3 (which faultlab commit moves the reclaim orphan count); CI at efa5214b (A3)
Quest: .campaign/xapiand/quests/faultlab-adopt-strata.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Migrated at strata:e8645287 (pushed): CMakeLists fetches faultlab e3fcfe3 and `link_libraries(faultlab::faultlab)`
in the test block; 49 files now `#include "faultlab/faultfs.h"` and use `namespace fl = kronuz::faultlab`
(alias in the including header where a header carries it: kv_test.hh, counted_hooks.hh,
group/host_io.hh, group/oracle.hh); faultfs.hh and faultfs_test.cc deleted; STRATA_FAULTFS_MAP is
FAULTLAB_MAP (CMakeLists, three tests); AGENTS.md and README.md. bench/kv/kv_strata.hh has only a
comment naming faultfs: no change (turn-log-parity merge is then only standalone_turn_log_test.cc
and CMakeLists). Jobs: 261121-040338-57550f (A1 counts at head), 261121-040340-abd200 (A1 counts at
main origin/main 753bc62c, slot adopt-main), 261121-040345-ee15e1 (A1 suite at head),
261121-040455-3c1deb (A2). The count script is strata test/crash_counts.py (copy at
~/.scratch/faultlab-adopt-strata/crash_counts.py, which both count jobs run).

## Next

1. Suite 261121-050042-88219e passes, CI green at efa5214b: A1 suite and A3.
2. Bisect 261121-dd30d3 names the faultlab commit behind the +3 truncated orphans (below); write it
   under Evidence A1.
3. A2 passed at afda6e61 (261121-040455-3c1deb): docs added to the file set by the lead.

Fixed at strata:efa5214b (pushed): the suite and macOS CI failed on bench/field_table/field_table.cc
(`namespace fl` sat inside the non-Apple #else); Linux CI failed on counted_io (probe_io did not
forward open_reclaim_candidate, so its open_existing default threw ENOENT for a missing name).

Count diff, main (261121-040340-abd200) vs head (261121-040338-57550f): 38 of 40 lines identical
(seeds, crash points, manifests checked, pass states). Moved: `reclaim` 2835 -> 2838 and
`reclaim_bg_crash` 2735 -> 2738 truncated orphans seen; crash points (3128, 3094) unchanged; the
mapped variants unchanged. Cause pending the bisect job.

## Blocked on

Lead: A2's grep also matches `include/strata/core/faultfs.hh` in docs/STATUS.md:13 and
docs/book/OUTLINE.md:371,393,394,830 (docs are not in this quest's file set). Options: (a) add
docs/STATUS.md and docs/book/OUTLINE.md to the file set and I point them at faultlab/faultfs.h;
(b) narrow A2's exclusions. Recommend (a). Other docs name `faultfs.hh:LINE` without `core/`
(design/layers, shared-reader, simulator, cold-tier, standalone-turn-log, mechanisms/defer-io): not
matched, left as history.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | jobs above (pending) |
| A2 | 261121-040455-3c1deb (expected to fail on the four docs hits until Blocked on is answered) |
| A3 | CI after the push |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- counted_io_test: a reclaim-candidate open of a missing name is expected null (its own comment
  said so; strata's copy threw ENOENT, faultlab settles io.h's contract: faultlab-strata-gaps A3).
- One alias, `fl`, everywhere (journal and consensus use it); the five `ff` files moved to it too.
  voters_test.cc has a local variable `fl` that hides the alias inside that function; harmless.
- `link_libraries(faultlab::faultlab)` once in the test block rather than per target (about 100).
