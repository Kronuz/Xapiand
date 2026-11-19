# stopword-lists: journal

State: waiting on jobs 261119-082832-0fe0b6 261119-082834-ac95e7 261119-082835-32cd27 261119-082841-e595c8
Quest: .campaign/xapiand/quests/stopword-lists.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Built-in lists, overrides, fingerprint API, refusal for unknown languages and the docs are committed
(quest/xapiand/stopword-lists: c7e5061ac probe, 16ef67206 code and tests, ae39eb0f9 docs). The index-metadata
recording (A3 integration) needs files outside the set: see Blocked on. Jobs are queued; read their logs.

## Next

1. Read jobs: tests 261119-082832-0fe0b6 (A1-A4), mutant 261119-082834-ac95e7 (A3 fails-without, at the
   branch mutant/stopword-lists-fingerprint 93f14fe71: delete that branch after), probe before 261119-082841-e595c8
   (c7e5061ac) and after 261119-082835-32cd27 (A5). Summarize the A5 medians (awk over the probe lines) here.
2. After the lead widens the set: record and check the fingerprint in the schema (see Blocked on), then the suite (A7).

## Blocked on

The lead: widen the file set for the A3 integration (the API is done: `stopWordsFingerprint(lang)` at
index time, `checkStopWords(index, lang, recorded)` on open and query; both throw as the quest asks).

1. `src/reserved/schema.h`: a reserved property name, e.g. `RESERVED_STOP_FINGERPRINT` (`_stop_fingerprint`).
2. `src/database/schema.cc`: record it beside `mut_properties[RESERVED_LANGUAGE]` (line ~4997, a text field
   with a language: `stopWordsFingerprint(language)`); check it where the field's stored properties become a
   specification (lines ~9276, 9553, 9608, 9708, next to `res.language = ...`) with
   `checkStopWords(index, res.language, recorded)`. The schema is the index's persisted metadata
   (RESERVED_SCHEMA), so this is the "persisted key". Open question for the lead: an index whose fields have a
   language but no recorded fingerprint (built before this change) is let through and then records one on its
   next schema write, or refused; formats are open until release, so refusing is allowed.
3. `src/multivalue/generate_terms.cc:534`: `getStopWords(language)` now throws `UnknownStopWordsError`
   (a `std::invalid_argument`); map it to `engine::Error(errc::invalid, ...)` as `getStemmer` does at ~516.
4. `tests/test_control_plane.cc:544`: `CHECK(getStopWords("yy") == nullptr)` becomes
   `CHECK_THROW(getStopWords("yy"), UnknownStopWordsError)`. Without it the suite (A7) fails on that line.
5. `CMakeLists.txt:1318-1319` (`install(DIRECTORY stopwords ...)`): with the lists compiled in, the install
   copies 18 files that a node then reads at start-up as overrides (the same bytes; cost is the "files18" rows
   of the probe). Drop the install rule so a default install reads nothing; the repo's `stopwords/` stays as
   the source of the generated `builtin.inc`. Also `docs/mechanisms/` and `docs/perf/` are outside the set:
   where should the how-it-works note and the A5 numbers go?
6. Decision for the lead (changes a contract: the quest's "perfect-hash sets"): see Rulings I made 1.

## Evidence

Jobs prove criteria by their tags (`--for A1`); `campaign brief` generates the table.

| Criterion | Evidence |
|---|---|
| A1 | job 261119-082832-0fe0b6 (scenario `builtin`: 18 shipped files vs compiled lists, counts, fingerprints, no data directory) |
| A2 | job 261119-082832-0fe0b6 (scenario `override`: en replaced, xx custom, fr same words same fingerprint, de untouched, files deleted after preload; `unreadable` throws) |
| A3 | job 261119-082832-0fe0b6 (unchanged passes, changed and missing refuse with index, list and both fingerprints) and 261119-082834-ac95e7 (the same test fails at the mutant). The integration into the schema is Blocked on 1-2 |
| A4 | job 261119-082832-0fe0b6 (`getStopWords`, `stopWordsFingerprint`, `parse_text` with an unknown language throw naming it); generate_terms.cc is Blocked on 3 |
| A5 | jobs 261119-082841-e595c8 (before) and 261119-082835-32cd27 (after); numbers under Result |
| A6 | `docs/src/content/docs/reference-guide/search/query-dsl/leaf-queries/text-queries/stop-words.md`, `docs/src/content/docs/changelog.md` ([Unreleased] / Changed) |
| A7 | pending Blocked on 4 |

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
