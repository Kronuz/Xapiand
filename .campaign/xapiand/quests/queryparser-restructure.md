# queryparser-restructure: queryparser: the parser's operator, word and group layers in headers of their own

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status queryparser-restructure`.

## Outcome

`queryparser.cc`'s `class parser` (line 218, 1,047 lines) is one recursive-descent parser in three
layers, already apart in its file order:
- operator recognition (lines 221-475): keywords, NEAR/ADJ chains, `binary_keyword`, `op_kind`;
- the word layer (476-905): globs, fuzzy suffixes, word groups, CJK n-grams, phrases, stopwords and
  terms;
- grouping and the tree (906-1232): frames, `push_item`, `combine_pass`, `join`, `fold` and
  `reduce`.
Its scanner and tree builder are already subobjects. The class declaration moves into an internal
header (`queryparser/parser.hh`), and each layer's definitions into a header of its own
(`queryparser/parser_operators.hh`, `parser_words.hh`, `parser_groups.hh`), all included only by
`queryparser.cc`. It stays one translation unit, so inlining and codegen stay the same. A word bug
(wildcards, CJK) and a precedence bug then open different files.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, and CI on every platform ([ci])
- A2 The move changes no code: the `members` check passes, and the journal answers each warning.
  A Release `tools/fnsizes.sh` diff of the test binaries, base against head, in one job, shows no
  function whose size changed, or each difference explained in the journal
- A3 Every new header compiles alone: `tools/selfcontained.sh` over the new headers
- A4 [artifact] `docs/source-layout.md` lists each header and its layer
- A5 [manual] The lead reads the cut and agrees each header is one layer a maintainer opens alone

## Contracts

queryparser's public API and `queryparser.cc`'s translation unit stay the same.

## Decisions

- Owner, 2026-11-21, on the class bodies over 1,000 lines: "These files would have to be carefully
  split. I would still split them with craftmanship and aiming at improving maintainability and
  parallel work. Decide which are worth and how to split them cleanly."
- Lead, 2026-11-21: worth it for maintainability; parallel work on it is low (13 commits in 30 days,
  one quest at a time). It needs split-queryparser, which holds queryparser.cc. Low risk: a
  mechanically proved move in one translation unit.

## May land early

Nothing: one landing.
