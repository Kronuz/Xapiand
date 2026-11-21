# Test layout

A test that grew past 1,000 lines is split by what it tests into pieces in `tests/test_NAME/`.
`tests/test_NAME.cc` stays: it holds the run list (`TEST_MAIN`, or the `main` it had) and the
one-time setup, and CMake builds it as `xapiand_test_NAME`, registered as `test_NAME`. The
`tests/test_*.cc` glob does not descend, so a piece is never a test of its own. One rule in the
tests block of `CMakeLists.txt` adds `tests/test_NAME/*.cc` to `xapiand_test_NAME`.

Each directory has the same shape:

- `support.h` holds what several pieces share in `namespace detail`: the types, the model, and
  each shared helper as a whole `inline` definition (default arguments on it), plus one
  declaration per test function.
- Every piece opens with a one-line comment saying what it holds, includes `support.h`, and
  defines its tests in `namespace detail`. A helper only one piece uses stays in that piece, in an
  anonymous namespace.
- A namespace-scope object with a dynamic initializer stays in a `.cc`, declared `extern` in
  `support.h` when a shared helper uses it: in `tests/test_NAME.cc`, or in the piece whose tests
  use it.

No file is kept whole over 1,000 lines: the four tests were each split, and every piece is a test topic opened alone.

## test_lua

| File | Holds |
|---|---|
| `tests/test_lua.cc` | The watchdog and the run list (order matters: the last test starts a node) |
| `tests/test_lua/tests.h` | One declaration per test function |
| `tests/test_lua/support.h` | The call helpers (`Outcome`, `call`, `start`, `Pending`, `quick`, `other_requests`) |
| `tests/test_lua/results.cc` | What a call returns: the document, the converted result and its bounds, `print` |
| `tests/test_lua/environment.cc` | A call's own environment: interleaved calls, cached scripts, garbage, the string table; `short_strings_800k` |
| `tests/test_lua/bounds.cc` | The instruction, callback, sort, pattern and allocation caps |
| `tests/test_lua/bounds_hooks.cc` | Bounds the instruction hook cannot see: close handlers, library work charged by what it touches |
| `tests/test_lua/script_names.cc` | Local names, foreign references and links, name-only requests |
| `tests/test_lua/slices.cc` | Slices: other work runs during a long call, a cancelled call stops |
| `tests/test_lua/heavy_lane.cc` | A call runs in the heavy lane and the request keeps its lane |
| `tests/test_lua/method.cc` | A script reads the method of its write in `_method` |

## test_engine_search_strata

| File | Holds |
|---|---|
| `tests/test_engine_search_strata.cc` | `guarded`, the per-test wrappers and the run list |
| `tests/test_engine_search_strata/support.h` | The model and its corpus, the model's answer (matching documents, BM25 weights), running a query and checking it against the model, random queries, and the test functions |
| `tests/test_engine_search_strata/shards.cc` | One shard, several shards, positional queries, appends, the spy |
| `tests/test_engine_search_strata/refusals.cc` | What the engine refuses |
| `tests/test_engine_search_strata/documents.cc` | Writes and deletes, phrase alternatives, fields |
| `tests/test_engine_search_strata/reads.cc` | Long chains, concurrent searches, snapshot consistency |
| `tests/test_engine_search_strata/views.cc` | A search's view holds nothing; a failed reopen is pinned |
| `tests/test_engine_search_strata/windows_collapse.cc` | Result windows, percent cutoffs, collapse |

## test_engine_query

| File | Holds |
|---|---|
| `tests/test_engine_query.cc` | The stop-word data path, the `GUARDED` wrappers and the run list |
| `tests/test_engine_query/support.h` | Running a query on a shard, a shard of terms, the generator |
| `tests/test_engine_query/shape.cc` | Leaves, operators, emptiness, expansions, texts, patterns |
| `tests/test_engine_query/sources.cc` | Value sources, the bytes they read, a stored value that does not decode |
| `tests/test_engine_query/callbacks.cc` | Posting sources, key makers, match spies, who frees what a search makes |
| `tests/test_engine_query/deep.cc` | Queries as deep and as long as their caps |
| `tests/test_engine_query/dsl.cc` | The query DSL: not-is-none-of, keyword values |
| `tests/test_engine_query/range_aggregation.cc` | Range aggregations with open bounds |

## test_engine_callbacks

| File | Holds |
|---|---|
| `tests/test_engine_callbacks.cc` | The corpus's `words` and the `main` that runs the tests selected by `ONLY` |
| `tests/test_engine_callbacks/callbacks.h` | The value slots and the key makers, spies and posting sources the tests hand to a search |
| `tests/test_engine_callbacks/support.h` | The corpus written to shards and to one merged shard; the model (which documents a query matches, wildcard and edit-distance expansions); running a search and comparing it with the model |
| `tests/test_engine_callbacks/search_callbacks.cc` | Key makers, spies, posting sources: sorter, spy, sources |
| `tests/test_engine_callbacks/operators.cc` | Boolean terms, single documents, appends, operators, expansions |
