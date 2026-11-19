# stopword-lists: journal

State: blocked on lead: schema recording needs schema.h and handler.cc (see Blocked on); jobs 261119-083429-63dc87 261119-083432-825a1e 261119-083435-9bba38 261119-083443-f71030 queued
Quest: .campaign/xapiand/quests/stopword-lists.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead rulings 1-7 applied except the schema recording (ruling 1, see Blocked on): generate_terms.cc catches the
throw, test_control_plane expects it, the install copies the lists to share/doc/xapiand/stopwords and the build no
longer copies them to the read directory, the how-it-works note is docs/mechanisms/stopword-lists.md. Head 2bba60a2f.

## Next

1. Read the jobs below. Write docs/perf/stopword-lists.md from the A5 probe (medians per mode, before vs after);
   decide ruling 7: if start-up or lookup cost is material, add a hunt-row candidate for a view mode in
   analysis::stopper (hunt.md is the lead's: write it in this journal for the lead), else drop the idea with numbers.
2. After the lead answers the schema question: record and check, adopt for indexes with no record, then
   `campaign verify stopword-lists` (A7).

## Blocked on

The lead: ruling 1 needs two more files and one design choice.

Facts found: `Schema` has no index name (its `origin` is the foreign schema's path, empty for a local index), so the
error "naming the index" cannot be raised inside schema.cc; `DatabaseHandler::get_schema` (src/database/handler.cc:375)
knows `endpoints`. A new `_stop_fingerprint` property in a field's stored properties also needs `schema.h`
declarations (feed/process/consistency/readable functions: the property tables at schema.cc:6198, 6340, 6365, 6474,
6550, 9188 list every reserved key) and shows up in every schema output (to_string, e2e fixtures with a text field
and a language) unless `dispatch_readable` hides it.

Options:
A. (recommended) Field property `_stop_fingerprint` next to `_language` (recorded at schema.cc:4999 when the field
   is created), hidden from the readable schema. `Schema::check_stop_words(index)` (new, schema.h) walks the
   fields' properties and calls `checkStopWords`; `DatabaseHandler::get_schema` calls it with the endpoints' name
   after building the Schema, and the same call adopts the current fingerprint for a field that has a language but
   no record (ruling 6; persisted with the next schema write, one info log line). No extra metadata read: the
   fingerprints ride in the cached schema. Files: src/database/schema.h, src/database/handler.cc (+ the already
   widened schema.cc, reserved/schema.h).
B. A separate metadata key `_stopwords` = {language: fingerprint} written with `set_metadata` and read by
   handler.cc on open: one extra metadata read per open unless cached beside the schema, and the languages used
   must be found by walking the schema anyway. Files: src/database/handler.cc (and schemas_lru.cc for the cache).
Which one, and widen: src/database/schema.h, src/database/handler.cc.

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
