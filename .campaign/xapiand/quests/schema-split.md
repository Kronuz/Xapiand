# schema-split: schema.cc becomes a set of files under 1,000 lines each

Risk: low (a behavior-preserving refactor). The lead writes this quest; contributors read it and never
edit it. Its machine state is in `state.json`: `campaign status schema-split`.

## Outcome

`src/database/schema.cc` (10,076 lines at Xapiand:7d185f626) is split by responsibility into files
under `src/database/schema/`, each under 1,000 lines, with `src/database/schema.h` kept as the one
include for callers (or split the same way when it is large enough to need it). The grouping
follows what the code does (for example: field specification and its parsing, type detection and
validation, indexing of values, namespaces and paths, serialization, the reserved-word dispatch
tables), each file with a one-line header comment saying what it holds. No function changes: moves,
the includes each file needs, and `static` helpers made file-local or shared through a private
header. Sources are globbed (CMakeLists.txt:1122), so no CMakeLists change is needed. Owner,
2026-11-21: "Source code files should be much smaller and well organized."

## Acceptance criteria

- A1 The suite passes at the split code: the `suite` check
- A2 No file under `src/database/schema*` exceeds 1,000 lines, and every function body moved
  unchanged: the `split` check (cpp-libraries' `tools/split_check.py`, quest split-check), with its
  warnings answered in the journal
- A3 [artifact] `docs/dev/schema-layout.md` lists each file and what it holds

## Contracts

None: no behavior, API or format changes.

## Decisions

- Lead, 2026-11-21: 1,000 lines per file, the default the owner was offered.

## May land early

None.
