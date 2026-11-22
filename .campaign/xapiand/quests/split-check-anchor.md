# split-check-anchor: split_check matches repeated lines by their unique anchors

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status split-check-anchor`.

## Outcome

`tools/split_check.py` matches each added line to the first equal removed line its pass meets, so a
line repeated across a file (`}`, `inline bool`, a license's `/*`) takes the wrong source and a
verbatim move reads as a cut. schema-split's check at Xapiand:a6d3eae reported 40 errors this way,
every one a line moved verbatim. Before the line-by-line pass, the checker now pins the runs around
anchors: lines whose text is removed once and added once, extended over neighboring non-blank lines
while both sides read the same (the idea of patience diff).

## Acceptance criteria

- A1 A fixture whose repeated lines (`}`, a shared signature line) sit inside verbatim moves passes,
  and a real cut inside a function body still fails, naming the function: the `test` check, with
  the new tests failing on the checker before the change (a job at the base)
- A2 Ruff check and format pass on the touched tools, and every existing tool test passes: the
  `lint` and `test` checks
- A3 schema-split's split at its head reads 0 errors with the changed checker: a job naming the
  schema-split commit

## Contracts

`split_check.py`'s command line and exit codes stay as they are.

## Decisions

- Lead, 2026-11-21: low risk (tooling). The patch is schema-split's contributor's
  (~/.scratch/xapiand/schema-split/split_check_anchor.patch); the lead applies it and reads it.

## May land early

None.
