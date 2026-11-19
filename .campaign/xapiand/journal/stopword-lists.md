# stopword-lists: journal

State: waiting on jobs 261119-084153-197847 261119-084158-c63615 261119-084202-4abafa 261119-084205-cfa578 261119-083443-f71030 261119-084208-75bb78
Quest: .campaign/xapiand/quests/stopword-lists.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Option A is implemented (head 7b12e5d5e, master merged): `_stop_fingerprint` recorded beside `_language`,
`Schema::check_stop_words(index, adopt)` called from `DatabaseHandler::get_schema`, property hidden from the readable
schema, options.md fixed, mechanism note updated. Tests are in tests/test_stopword_lists.cc (schema checks in the
`builtin` and `override` scenarios, `--probe schema`).

## Next

1. Read the jobs: tests 261119-084153-197847 (A1-A4), mutant 261119-084158-c63615 (A3 fails-without, branch
   mutant/stopword-lists-fingerprint, delete after), A5 after 261119-084202-4abafa, before 261119-083443-f71030,
   schema-check cost 261119-084205-cfa578, suite 261119-084208-75bb78 (A7).
2. Write docs/perf/stopword-lists.md from the probe medians (start-up us, heap bytes, RSS; schema check ns per
   call); ruling 7: if material, a hunt-row candidate for a view mode in analysis::stopper, else drop with numbers.
3. Merge master again before the last jobs; fix what the suite shows.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); `campaign brief` generates the table.

| Criterion | Evidence |
|---|---|
| A1 | job 261119-083429-63dc87 (scenario `builtin`: 18 shipped files vs compiled lists, counts, fingerprints, no data directory) |
| A2 | job 261119-083429-63dc87 (scenario `override`: en replaced, xx custom, fr same words same fingerprint, de untouched, files deleted after preload; `unreadable` throws) |
| A3 | job 261119-083429-63dc87 (unchanged passes, changed and missing refuse with index, list and both fingerprints) and 261119-083432-825a1e (the same test fails at the mutant). The mutant is branch mutant/stopword-lists-fingerprint (11d4f7004), to delete after. The schema integration is Blocked on |
| A4 | job 261119-083429-63dc87 (`getStopWords`, `stopWordsFingerprint`, `parse_text` with an unknown language throw naming it); generate_terms.cc maps the throw to an invalid engine error |
| A5 | jobs 261119-083443-f71030 (before) and 261119-083435-9bba38 (after); numbers under Result |
| A6 | `docs/src/content/docs/reference-guide/search/query-dsl/leaf-queries/text-queries/stop-words.md`, `docs/src/content/docs/changelog.md` ([Unreleased] / Changed) |
| A7 | pending the schema recording; `campaign verify stopword-lists` after |

## Result

Pending the jobs.

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
