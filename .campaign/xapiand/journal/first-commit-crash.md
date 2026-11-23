# first-commit-crash: journal

State: working
Quest: .campaign/xapiand/quests/first-commit-crash.md (the lead's; never edit it)

## Doing

Targeted lifecycle_test at the fix (261123-045330-1cff01) and the fails-without at the test-only
commit (261123-045326-9bf051) are running. Then: fill the doc's cost numbers from their output,
commit the doc, run the suite (A1, A3) and the mutants (A4).

## Next

1. Read both jobs' full logs (~/.scratch/campaign/jobs/<id>/a1/log): FAIL lines at main, the sweep
   summary lines at the fix (mutations, syncs).
2. Put the measured cost into docs/mechanisms/manifest-log-file.md "The first manifest"; commit.
3. `campaign verify first-commit-crash --for A1,A3`; mutants as plain `--quest` jobs (A4).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <suite at head, pending> |
| A2 | 261123-045326-9bf051 (fails-without at strata:c18becbc, test only), pending |
| A3 | <suite at head, pending> |
| A4 | <mutants, pending> |
| A5 | docs/mechanisms/manifest-log-file.md, "The first manifest" and "Files no manifest names" |

## Result

<pending>

## Rulings I made

- Ordering, not recovery. A directory holds an index exactly when it holds a manifest log; the
  first manifest goes before any data file. Recovery stays as it is, so the existing lifecycle
  check (a committed index whose logs were removed is corrupt_index, and create refuses it) holds
  unchanged (A3), and nothing is ever deleted from a directory with no log but a lone
  `manifest-1.tmp`. Recovery's alternative ("no log means uncreated, whatever segments it holds")
  would have read that damaged index as never created and let create delete its segments.
- The hook is in `flush()`, not `commit()`: refresh() and a budget flush also write a deferred
  index's first segment before any commit, the same window. Condition: no generation committed and
  documents buffered, so create() and a first commit with nothing buffered still write one
  generation.
- The empty generation is the state create() commits (UUID, configured, the join's attributes,
  nothing applied), never the applied scalars, metadata, schema or fields: those would make a cut
  reopen a partial state.
- install()'s deferred path (group-heal's F2) is left to group's activate(), which commits first:
  the lead scoped this to the commit and recover path.
