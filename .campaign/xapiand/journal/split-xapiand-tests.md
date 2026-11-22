# split-xapiand-tests: journal

State: waiting on jobs 261122-002100-ba4b13 (split check), 261122-002112-733a32 (A3), 261122-002116-2b5980 (A6) at ccef874, and CI (lead, 00:21: master merged again after a transient push error stopped the landing loop at 21:33)
Quest: .campaign/xapiand/quests/split-xapiand-tests.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 2 (lead's read of 83dbe650e): every shared helper now lives whole in its directory's
support.h as an `inline` definition in `namespace detail`, copied from the old file by line range
with only `inline` added, defaults on the definition; the helper-only pieces are gone
(callbacks corpus/model/run.cc, strata corpus/answer/compare.cc, query support.cc, lua
support.cc). This also fixes the suite's compile failure (261121-121046-87774b: `datas_of`,
defined in run.cc's anonymous namespace, called from search_callbacks.cc:72): it is in support.h.

## Next

1. Read the four jobs; fix any failure; set State to ready for review.

## Blocked on

Nothing. The `split` gate check fails until quest split-check-sources lands (the landed tool reads
non-C++ files); A2 runs that branch's tool meanwhile.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | suite 261121-213323-023440 at 19820a501 (replaces 261121-121046-87774b, which failed to compile at 83dbe650e: `datas_of` undeclared in search_callbacks.cc) |
| A2 | 261121-213323-00b7d8 (rerun at 32c0358 of 261121-133857-a00ef2): split-check-sources' split_check.py --base c2bc67fac tests (a local run at 19820a501: 0 errors, 216 warnings, answered below); gate 261121-213323-00b7d8 waits on the landed tool. Earlier: 261121-114927-58c8e2, 261121-121047-65d7d0, 261121-123005-b98fae failed on non-C++ files (the landed tool) |
| A3 | head 261122-002112-733a32 (at ccef874), base 261121-114945-35c995 (at e5f7765d1): diff only their `Test #` and `Total Tests` lines |
| A4 | docs/dev/test-layout.md |
| A5 | CI on the pushed branch at 19820a501 or later |
| A6 | 261122-002116-2b5980 (at ccef874): each of the six new headers included twice in an otherwise empty TU, -fsyntax-only, with the flags compile_commands.json gives a piece of its directory (script ~/.scratch/xapiand/split-xapiand-tests/selfcontained_new.py; cpp-libraries' selfcontained.sh skips tests/ directories, so it is the same check, driven by hand) |
| A7 | lead, 2026-11-21: read the split; four test programs split by topic into TUs with support headers, each piece opens with what it holds |

### split_check warnings, answered (local run at 19820a501, base c2bc67fac)

- 61 `added 'inline'` (support.h of all four): the shared helpers, defined whole in the header;
  the linkage change is listed under Rulings.
- 115 `namespace-scope declaration added` for `void test_...();` (tests.h, support.h): one
  declaration per test function, so the run list in tests/test_NAME.cc sees the pieces' tests.
- 2 `namespace-scope declaration added` for `extern const ... words;` (callbacks support.h) and
  `extern const std::string short_strings_800k;` (lua support.h): the objects with dynamic
  initializers stay in a .cc, unchanged; the prior `extern` declaration gives them external
  linkage.
- 22 `dropped 'static'` (test_engine_query pieces): the `static void test_x()` functions, now
  called from tests/test_engine_query.cc; listed under Rulings.
- 4 `using added`: `using namespace detail;` in each tests/test_NAME.cc, so the run list names
  the tests as before.
- 7 `anonymous namespace opened in new code` (strata reads/refusals/views/windows_collapse.cc,
  lua environment/heavy_lane/method.cc): helpers local to one piece keep internal linkage, as
  they had in the old file's anonymous namespace.
- 2 `removed include does not reappear`: `engine_support.h` (query) and `utils_node.h` (lua)
  reappear in support.h spelled `../engine_support.h` and `../utils_node.h`.
- 2 `removed comment does not reappear` (callbacks): the section dividers `// --- callbacks ---`
  and `// --- tests ---`; callbacks.h and each piece open with the comment saying what they hold.
- 1 `dynamic initializer moved` (lua method.cc:148, `const std::string got_ = (got); \`): a false
  positive, a block-scope local inside the `CHECK_SEEN` macro body; reported to the lead for the
  split-check-sources quest.

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Linkage changes, all listed:
  - Shared helpers, added `inline` and moved whole into support.h, out of the old file's anonymous
    namespace into `namespace detail`: callbacks (pad, path_of, random_spec, key_of, make, build,
    has_term, dictionary_of, code_points, glob, distance_of, expand, expected_of, run, close,
    data_of, weights_by_data, datas_of, check_ranking, both, same_documents, t, key_value,
    group_value); strata (path_of, make, random_doc, write_docs, build, id_of, phrase_in, near_in,
    statistics_of, bm25_of, bm25, synonym_weight, edit_distance_of, add_positions, slot_in,
    phrase_of, near_of, weigh, model_hits, run, close, ids_of, compare, random_query,
    random_queries); query (ranked, run, terms_db); lua (outcome_of, doc_of, call x2, start,
    absent, str, quick, other_requests). The helpers a shared helper calls go with it, since an
    inline definition can't call a function local to one translation unit. Templates
    (error_kind, throws_as) and types move as they were.
  - Test functions shared with the run list: out of the anonymous namespace into `namespace
    detail`, declared in support.h (tests.h for lua); in test_engine_query the 22
    `static void test_x()` became `void test_x()`.
  - `words` (callbacks) and `short_strings_800k` (lua), objects with dynamic initializers, keep
    their old lines unchanged in a .cc (tests/test_engine_callbacks.cc; lua environment.cc, its
    old place among the tests that use it) and gain external linkage from an `extern` declaration
    in support.h, which the shared helper that uses `words` needs.
- Helpers local to one piece stay in that piece inside an anonymous namespace
  (-Wmissing-declarations is on).
- Dynamic initializers stay in the test's own .cc: the lua watchdog and the query test's
  `data_path` (setenv).
- The lua forward declaration of `other_requests` (old line 1024) stays, unchanged, in
  support.h just before its definition: a removed declaration must reappear, and
  -Wredundant-decls rules out leaving it after the definition in bounds_hooks.cc.
- support.h keeps the helpers in the old file's order, so each header is a few contiguous slices
  of the old file.
- Piece headers include `../test_harness.h` and `../engine_support.h`: the pieces are one
  directory below tests/.
- Checked against cpp-libraries docs/SPLITTING.md: pieces are separate translation units with a
  support header, each a test topic; with support.cc gone no piece is under 100 lines. callbacks.h
  holds the slot constants it uses, so it is self-contained.
