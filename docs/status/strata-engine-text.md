# strata-engine-text (phase 7a, S4: the text node through queryparser)

Branch `phase7a/strata-engine-text` from Xapiand master `7efc3753e` merged with `phase0.5/queryparser-ab`
(`c310e0568`). Local only, not pushed. Design: `docs/design/xapiand-on-strata.md` row S4 and D6 (strata);
`docs/design/queryparser.md` (branch `design/queryparser`) Q4.

## State

In progress. queryparser is the only text parser and runs on the host: `engine::parse_text`
(`src/engine/text.{h,cc}`) reads a field's query string into an `engine::Query`. The `Text` node is gone
from the seam (`Query::text`, `QueryKind::text`, the Xapian `QueryParser` path, `XAPIAND_QUERYPARSER` and the
A/B build). Mechanism note: `docs/mechanisms/text-queries.md`.

## Decisions taken here (flag any the lead wants otherwise)

1. Error kinds: syntax, `unsupported` (NEAR/PHRASE over phrases) and `too_long` all become
   `QueryParserError` (`errc::syntax`, a client error, 400). `unsupported` was `Xapian::UnimplementedError`,
   which the old translation classed as `errc::internal` (500): a client mistake answered as a server error.
   `out_of_memory` is `std::bad_alloc`. Settings the parser cannot read (an unknown stemmer language, a
   default operator it has no word for) are `InvalidTextConfigError` (`errc::invalid`).
2. `WORD_BREAKS` (`cjk_words`, ICU builds only) is not read: CJK text on such a field is split like any other
   text. Latin text is unaffected. Indexing with `cjk_words` still uses Xapian's term generator
   (`src/engine/xapian/text.cc`), so a CJK query on such a field no longer matches word-break terms. Open for the
   lead: an ICU break-iterator segmenter in `engine/text.cc` closes it; the docs suite has no `cjk_words` query.
3. queryparser pinned at `09a4ea6` (main: the merit decisions "2.1 versus 2.0" recorded).

## Done

1. `Xapiand:f29dfe942` host-side `engine::parse_text`, Text node removed, errors mapped, changelog and
   mechanism note (`docs/mechanisms/text-queries.md`). Pin queryparser `09a4ea6`.
2. Test braces commit after it. Proven so far, ASan+UBSan build `.scratch/build-asan` (Apple clang):
   `xapiand_test_text` 7165 checks (706 A/B decisions as recorded; lowering equals the tree on ~5,500
   vector rows), `xapiand_test_engine_query` 22395 checks (live agreement with Xapian's parser),
   `seam_check.sh` green (7 files, 158 lines: unchanged; the text path never appeared in the allowlist).
3. Finding: `_near` over `(barack OR michelle) AND obama` (no BOOLEAN flag) is a QueryParserError: 400,
   asserted in `test_near_of_a_boolean_expression`. Stop words only: a group of only stop words keeps
   its words (as Xapian); only a text without words is empty.

## Doing / Next (resume here)

1. Run `.scratch/proof.sh` under `quiet light` (rebuilds, fuzz 2x150 s, five mutants of `src/engine/text.cc`
   via `.scratch/mutants.sh`; it restores the file with `git checkout`). Record the fuzz line and mutant
   failure counts here. Never edit `src/engine/text.cc` while it runs.
2. Rebuild `xapiand` (`.scratch/build-asan.sh xapiand`), run `.scratch/probe.sh .scratch/build-asan/bin/xapiand`
   (the finding's input over HTTP, expect 400 and no ASan report), then `.scratch/e2e.sh qp
   .scratch/build-asan/bin/xapiand` and expect 576/576 in `.scratch/e2e/qp.assert`.
3. Fix the changelog "Fixed" bullet to match the probe (400 for brackets under NEAR/PHRASE/ELITE_SET).
4. Delete `.scratch/build-asan` when done. Not done: E2E, fuzz run, mutants, ASan server probe.

## Blocked on

Nothing. Host hold queue was long (light slots full); each `quiet light` waited 10 to 30 minutes.
