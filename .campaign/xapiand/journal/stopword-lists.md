# stopword-lists: journal

State: ready for review
Quest: .campaign/xapiand/quests/stopword-lists.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All criteria have evidence at eae0478d9 (journal and docs-only commits after it). Ready for the correctness review.

## Next

1. Review findings, if any: each fix gets a test that fails without it, as a job.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261119-093906-b28504 (scenario `builtin`: 18 shipped files vs compiled lists, per-language counts, fingerprints, no data directory) |
| A2 | 261119-093906-b28504 (scenario `override`; `unreadable` throws) |
| A3 | 261119-093906-b28504 (unchanged passes, changed and missing refuse with index, list and both fingerprints, through `Schema::check_stop_words`) and 261119-093843-4ea7ac (the same test fails, 16 checks, at the mutant that makes `checkStopWords` return; branch deleted) |
| A4 | 261119-093906-b28504 (unknown language throws naming it in `getStopWords`, `stopWordsFingerprint`, `parse_text`) |
| A5 | 261119-083443-f71030 (before), 261119-084202-4abafa and 261119-093907-83563b (after); `docs/perf/stopword-lists.md` |
| A6 | `docs/src/content/docs/reference-guide/search/query-dsl/leaf-queries/text-queries/stop-words.md`, `docs/src/content/docs/changelog.md` ([Unreleased] / Changed) |
| A7 | suite 261119-093907-ca11c3 passed at eae0478d9; CI not read (the lead's) |

## Result

1. Lists compiled in (18 languages, 7,342 distinct words; the lead's 7,326 counted lines with comments and
   blanks, so the user docs say 7,342 and the test asserts the per-language counts from the files, no total).
2. First failure fixed: `_stop_fingerprint` showed in `to_string()`. `to_string()` is `get_full(true)`, the
   user-facing `_schema` of info and search responses (`search_views.cc:1347`), so that is the output tested.
   The cause was older: `Schema::dispatch_readable` does `switch (key)` on the raw hash against `_.fhh` cases
   (the other dispatchers use `_.find(key)`), so none of its cases ever match and the readable schema hid
   nothing on main either (`_prefix` and `_slot` show). The docs tests in
   `docs/src/content/docs/tests/schema/define-slot.md` expect `_prefix` and `_slot`, so I left the switch alone and
   erase the fingerprint before it. Lead's call: fix the switch (the readable schema then hides `_prefix`,
   `_slot`, `_acc_prefix`, and a `_stem_language` equal to `_language`) and update those docs tests, or leave it.
3. Schema check cost: `DatabaseHandler::get_schema` runs per request, so the check now remembers schemas that
   passed with nothing to adopt: 96 us (hot) to 563 us (cold) per check for 200 fields down to 0.17 us warm.
4. A3's old mutant job exited 1 on the two unrelated failures, so it proved nothing; the mutant was rebuilt at the
   fixed head and fails 16 checks. A5: no-data first use 105 to 142-214 us, heap 512 B to 29,176 B, "the" 0 to stopped.
5. No perfect hash (ruling 7), reason in the perf note.

## Rulings I made

1. No perfect hash in this change. constexpr-phf fits on compile cost (a 781-key set builds in about 0.7 s of
   clang at -O0, 570 keys the same; scratch probe, no job), but every consumer takes `const analysis::stopper*`,
   the analysis library's `unordered_set<std::string>` wrapper (pinned, outside this repo), so any compiled set
   is copied into one at first use whatever its layout; a PHF would sit behind that copy unused. The data is a
   sorted, NUL-separated blob per language (50,359 bytes in all, no relocations per word), whose bytes are
   also the fingerprint input. Making the PHF real needs a view mode in `analysis::stopper` (a lookup over the
   blob): the lead's call, with the numbers of A5 (heap bytes of one materialized list).
2. Fingerprint: FNV-1a 64-bit over the sorted unique words, each followed by a NUL, 16 hex digits.
   Not cryptographic; it detects accidental change. Compile-time for shipped lists, computed once for a file.
3. An unreadable override file throws (at preload, or at first lookup in a process without preload) instead
   of warning and using the shipped list: a quiet fallback is what D137 forbids.
4. `getStopWords` throws for an unknown language instead of returning null, so a caller that forgot the
   check cannot stop nothing silently. Messages: `Stop words list xx unknown` (400 on the query side) and `Index
   idx was built with stop words list en (fingerprint A), which has fingerprint B now` / `... which is missing now`.
5. Built-in lists are materialized per language on first use (`call_once`), not at start-up. `preloadStopWords()`
   now only reads the override files. Test scenarios run in one process each (the lists load once per process).
6. Observation, not changed (the owner keeps the lists, D97): `stopwords/tr.txt` spells some words with U+0163
   (ţ) where Turkish has ş (`ţimdi`, `ţu`, `ţöyle`).
7. The schema check cache is per thread and by schema identity (weak pointers, 4096 entries), because the
   schema cache that owns the versions (`schemas_lru.cc`) is outside the file set.
8. Lead's ruling 7 applied: perfect hash dropped; reason and numbers in `docs/perf/stopword-lists.md`.
