# hunt-header-tables: Header static tables: one copy and one initializer per program

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-header-tables`.

## Outcome

Hunt row 406. Headers that define `static const` objects with dynamic initializers give every
translation unit that includes them its own copy and its own initializer. split-xapiand-src's size
comparison at Xapiand:f6556e2 measured about 13 KB of `__GLOBAL__sub_I_*` per new serialize TU, from
`src/database/data.h`'s 11 `ct_type_t` objects (data.h:87 on), htm's 6 `start_vertices`, and the
soundex accent and composed tables (through keymaker.h and phonetic.h). Nine translation units on
master already pay it. This quest makes Xapiand's own tables one object per program (`inline const`,
or one definition in a .cc with an `extern` declaration where the type cannot be inline), and files
the htm and soundex parts as rows for their libraries, with the numbers.

## Acceptance criteria

- A1 The suite passes at the change: the `suite` check
- A2 A Release+LTO build's text and static-initialization bytes, before at the base and after at the
  head (cpp-libraries `tools/fnsizes.sh -m` on the xapiand binary, both as jobs), with the
  `__GLOBAL__sub_I_*` rows and the total, diffed. Name the honest magnitude, including zero
- A3 Startup is no slower: `xapiand --version` (or the earliest exit that runs every initializer)
  timed as a strict job at base and head, 30 runs each, with an A/A
- A4 [artifact] `docs/dev/header-tables.md`: each table, where it lives now, what it cost before and
  after, and the htm and soundex numbers handed to their libraries as hunt rows

## Contracts

None: no behavior, API or format change. The objects keep their names and values.

## Decisions

- Lead, 2026-11-21: low risk. Exploration first: confirm on current master, at most three
  experiments, then finish here if the change is a win.

## May land early

None.
