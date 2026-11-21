# schema-split: schema.cc split along its responsibilities

Risk: low (a behavior-preserving refactor). The lead writes this quest; contributors read it and never
edit it. Its machine state is in `state.json`: `campaign status schema-split`.

## Outcome

`src/database/schema.cc` (10,076 lines at Xapiand:7d185f626) is split by responsibility into files
under `src/database/schema/`, with `src/database/schema.h` kept as the one
include for callers (or split the same way when it is large enough to need it). The grouping
follows what the code does (for example: field specification and its parsing, type detection and
validation, indexing of values, namespaces and paths, serialization, the reserved-word dispatch
tables), each file with a one-line header comment saying what it holds. No function changes: moves,
the includes each file needs, and `static` helpers made file-local or shared through a private
header. Sources are globbed (CMakeLists.txt:1122), so no CMakeLists change is needed. Owner,
2026-11-21: "Source code files should be much smaller and well organized."

Follow cpp-libraries' `docs/SPLITTING.md` (cpp-libraries:7cab8ec).

## Acceptance criteria

- A1 The suite passes at the split code: the `suite` check
- A2 Every function body moved unchanged: the `split` check (cpp-libraries' `tools/split_check.py`, quest split-check), with its
  warnings answered in the journal
- A3 [artifact] `docs/dev/schema-layout.md` lists each file and what it holds
- A4 [manual] The lead read the split: each file is a unit a maintainer opens alone

## Contracts

None: no behavior, API or format changes.

## Decisions

- Owner, 2026-11-21: "The split must make sense and improve maintainability. Let's not split for
  the sake of splitting. For example, a 10K file might be ok if it makes complete sense to keep it
  that way." Cuts follow what the code does, never a line count; a part that is one coherent unit
  (a dispatch table, one parser) stays whole at any size, named with why in the layout note; no
  piece is a fragment included inside a function, class or namespace body.

## May land early

None.
