# prospect-completion-opens: journal

State: waiting on jobs 261112-015344-bac35a 261112-015356-09e3c0 261112-015401-c49945
Quest: .campaign/xapiand/quests/prospect-completion-opens.md (the lead's; never edit it)

## Doing

Cases at prospect:27ca2d9; lead's msg: merged prospect main (x7-scan-slices) and pinned strata d8ae10b8
(prospect:fcd9f3e, pushed; task pin bdafa5f already agrees, completion comes through strata's pin).
Running at fcd9f3e: build-test 261112-015344-bac35a (A1), A3 pass 261112-015356-09e3c0, A3 mutant
261112-015401-c49945.

## Next

1. Read the three jobs; check CI at fcd9f3e. Before the merge A1 passed via 261112-013520-bd7c93
   (ctest 51/51, churn and dump identical).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261112-015344-bac35a at fcd9f3e (pending); earlier job 261112-013520-bd7c93 (barrier-warning-strata A4 command at prospect:27ca2d9, strata:d8ae10b8, main 4c45e3dd): ctest 51/51, churn and dump identical |
| A2 | [artifact] `test/index_test.cc`: gate_io/wrapper_io/flaky_io forward all four read-only opens (prospect:ea89758; flaky_io at lines 112-115); not changed here |
| A3 | pass job 261112-013148-b915bd (format13, index, index_mapped pass; strata d8ae10b8); mutant job 261112-013153-51da31 (`--fails-without`: strata's first-map failure throws corrupt_index; all three abort) |

Baseline before the change: barrier-warning-strata's job 261112-003345-52611a failed exactly format13
:603/:676/:783, index :1399/:1795 (x5 errors), and the index_mapped twins.

## Result

format13_test.cc: `open_index` now maps the first segment after the open; a corrupt_segment there,
with needs_heal() and heal() == {id}, is the new verdict `damaged_segment` (cases :603, :676); the
rewritten-version case (:783) does the same inline. index_test.cc: the footer-byte case opens, the
first map throws corrupt_segment, heal() reports it; the flaky case opens (no segment read), the first
map's two failing reads give corrupt_segment for EIO (needs_heal true, heal() clears it) and the
host error unchanged otherwise (needs_heal false, the next map reads). Missing and resized-file
cases unchanged. eager_segments never set. Branch pushed.

## Rulings I made

- Mutant for A3 is strata-side (fail() throws corrupt_index): a copy of the strata ref in the job's
  out dir, so no checkout is touched. Script: ~/.scratch/prospect-completion-opens/run.sh.
- `open_index` maps the first segment for every verdict, so the `opens` cases also prove the
  segment maps.
