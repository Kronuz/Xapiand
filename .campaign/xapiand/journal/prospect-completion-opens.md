# prospect-completion-opens: journal

State: ready for review
Quest: .campaign/xapiand/quests/prospect-completion-opens.md (the lead's; never edit it)

## Doing

Six damaged-segment cases moved to the lazy default at prospect:27ca2d9 (pushed). A3's pass and
mutant jobs are done. A1 is barrier-warning-strata's A4 integration job rerun at 27ca2d9.

## Next

1. Nothing; A1 passed (ctest 51 of 51, churn and dump identical at t1 and t2).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261112-013520-bd7c93 (barrier-warning-strata A4 command at prospect:27ca2d9, strata:d8ae10b8, main 4c45e3dd): ctest 51/51, churn and dump identical |
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
