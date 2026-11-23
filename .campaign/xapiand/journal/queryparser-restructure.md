# queryparser-restructure: journal

State: waiting on jobs 261122-222826-4f317e 261122-222828-545da4 261122-222831-76a27b
Quest: .campaign/xapiand/quests/queryparser-restructure.md (the lead's; never edit it)
Code: queryparser worktree /Users/kronuz/code/Kronuz/queryparser-wt/xapiand/queryparser-restructure,
branch quest/xapiand/queryparser-restructure (pushed), base queryparser:0fdec0c (main).
Tools: cpp-libraries worktree /Users/kronuz/code/Kronuz/cpp-libraries-wt/xapiand/split-check-members
at cpp-libraries:2df69e8 (outline.py, split_check.py, fnsizes.sh, selfcontained.sh), never edited.

## Doing

The move is done in three commits: queryparser:b745b21 moves lines 33-1266 of queryparser.cc
(QUERYPARSER_CHECKS, `limit_exceeded`, `builder`, the item/frame/pending-op types, `parser`) into
`queryparser/parser.hh`; queryparser:3f5eda1 is outline.py's output over the map below (plus the
license step); queryparser:fc02cb5 the layout note. Its evidence jobs are queued.

## Next

1. Read the jobs (build-test 261122-222817-2abf44 passed, 8/8): members with the worktree tool
   261122-222826-4f317e (A2), fnsizes base/head in one job 261122-222828-545da4 (A2: explain each
   changed row here, or none), selfcontained 261122-222831-76a27b (A3). CI on the pushed branch
   (A1): `gh run list -R Kronuz/queryparser --commit fc02cb50cacb5198df1fb9024afe17cf335c0ee4`.
2. After split-check-members lands: rerun `campaign verify queryparser-restructure` so the
   `members` check (main's tool) passes; answer its warnings here.
3. The lead reads the cut (A5).

To redo the move (after a merge from main touches queryparser.cc): restore main's queryparser.cc,
rerun step A (lines 33-1266 into parser.hh, as in queryparser:b745b21), then
`outline.py apply queryparser/parser.hh MAP --inline` with the map below (its `@LINE`s are
parser.hh lines at step A: queryparser.cc line + 8), then the license step.

## The map (tools/outline.py, cpp-libraries:2df69e8)

```
# queryparser-restructure: the parser's members by layer (lines of queryparser/parser.hh at step A)
class: parser
umbrella-include: "queryparser/parser.hh"
piece-include: "queryparser/{}"
includes-comment: // The parser's member definitions, one header per layer.
keep-one-line: yes
parser_operators.hh: The parser's driver and operator recognition: the constructor, run() and step()'s dispatch, keywords, AND/OR/NOT/XOR, NEAR/ADJ and stray brackets. Included by parser.hh.
    parser run word_at fail fail_op@270 keyword op_word@290 op_keyword@293 proximity_word@296 slash_word step binary_keyword quote_before prox_rereads following_operator op_kind op_label stray_ket
parser_words.hh: The parser's word layer: globs, fuzzy suffixes, word groups, CJK n-grams, quoted phrases, stop words and terms. Included by parser.hh.
    take_word glob_word chain_follows glob_follows fuzzy_at joins glued_anchor word_group juxtaposed_word expands code_points reparse single ngram_run quoted stop_all@854 is_stop@855 stopword@856 finish make_term
parser_groups.hh: The parser's grouping and tree: items and frames, brackets, the combine pass over a group's items, joins, folds and operator reduction. Included by parser.hh.
    operand push_item open_frame close_frame close_group close_group_items join nary join_default join_run combine_items combine_pass fold gap_is_generators gap_is_space reduce
```

License step (`license.py REPO parser_operators.hh parser_words.hh parser_groups.hh`): the
repository's files open with the MIT block, then their comment, so each piece gets queryparser.cc's
22 license lines before outline's first-line comment, wrapped at 100 columns:

```python
"""Puts the repository's license block before each piece's first-line comment, wrapped at 100."""
import sys, textwrap
from pathlib import Path
repo = Path(sys.argv[1])
lic = (repo / "queryparser.cc").read_text().split("\n")[:22]
for name in sys.argv[2:]:
    p = repo / "queryparser" / name
    lines = p.read_text().split("\n")
    assert lines[0].startswith("// ") and lines[1] == "#pragma once", name
    about = textwrap.wrap(lines[0][3:], 97)
    p.write_text("\n".join(lic + ["// " + a for a in about] + [""] + lines[1:]))
```

Result: parser.hh 420 lines (class body 226-411, was 1,047 lines), parser_operators.hh 270 (18
members), parser_words.hh 446 (20), parser_groups.hh 349 (16), queryparser.cc 60. 54 member
definitions moved; 7 one-line accessors stay. Local split_check.py --members (2df69e8): 54 moved,
0 errors, 0 warnings; plain split_check at queryparser:b745b21: 0 errors, 0 warnings.

## Blocked on

Nothing. The quest's `members` check (261122-222818-a1a04e) runs main's split_check.py, which has
no --members until split-check-members lands: its failure is expected, not evidence; the worktree
tool's job 261122-222826-4f317e stands for it until then.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261122-222817-2abf44 at queryparser:fc02cb5 passed (8/8 tests); CI run 45124357782 on the pushed branch (queued) |
| A2 | members (worktree tool, cpp-libraries:2df69e8) 261122-222826-4f317e (queued); fnsizes Release, base queryparser:0fdec0c and head in one job with one FetchContent tree, library objects and the six test binaries: 261122-222828-545da4 (queued); quest check `members` 261122-222818-a1a04e fails until split-check-members lands |
| A3 | selfcontained.sh over every header (queryparser.hh, queryparser/*.hh), deps from a configured tree: 261122-222831-76a27b (queued) |
| A4 | `docs/source-layout.md` at queryparser:fc02cb5: each header, its lines and its layer |
| A5 | pending: the lead reads the cut |

## Result

<When the jobs are read.>

## Rulings I made

- R1 Layers follow the quest's line ranges: the constructor, `run()`, `word_at`, `fail` and `step()`
  (lines 221-475) go with operator recognition in parser_operators.hh, whose comment names them as
  the driver; `gap_is_generators` and `gap_is_space` (1204-1223) go with grouping.
- R2 Seven one-line state accessors stay in the class beside their declarations
  (`needs_reparse`, `boolean`, `phrase`, `lovehate`, `has`, `top`, `off`): every layer reads them.
  The one-liners that belong to a layer move with it (`fail_op`, `op_word`, `op_keyword`,
  `proximity_word` to operators; `stop_all`, `is_stop`, `stopword` to words).
- R3 parser.hh holds the builder and the group types with the class: the class needs them complete
  (`builder bld_`, `std::vector<item>`), so a self-contained parser.hh must; the builder alone (107
  lines) is not worth a fifth header. `QUERYPARSER_CHECKS`'s default moves with the builder, its
  only user; parse() catches `limit_exceeded` through the include.
- R4 The pieces open `namespace queryparser` only, not the anonymous namespace (outline.py drops an
  unnamed namespace): a member of a class in an unnamed namespace may be defined in an enclosing
  namespace, qualified. No anonymous namespace opens in new code, so split_check warns of none.
- R5 Definitions are `inline` (outline.py --inline), as split_check requires of a non-template
  header definition. clang adds an inline hint to a function declared `inline` and not to one
  defined in a class; the fnsizes job shows whether codegen moved.
- R6 Includes: parser.hh includes what it uses (queryparser.hh, unicode.hh, <cstddef>, <cstdint>,
  <cstdlib>, <string>, <string_view>, <utility>, <vector>); queryparser.cc keeps queryparser.hh and
  <new> and includes queryparser/parser.hh. Pieces include it by its full path, as queryparser.cc
  does. CMakeLists.txt is unchanged: the headers need no rule.
- R7 Each new header opens with the license block, then its comment, as every file in the
  repository does (prior restructures in other repositories open with the comment alone).
- R8 The map lives here, not in the repository: it is the record of a one-time move.
