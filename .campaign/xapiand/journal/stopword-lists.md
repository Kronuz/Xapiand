# stopword-lists: journal

State: waiting on jobs 261119-093906-670fdd 261119-093907-83563b 261119-093907-ca11c3
Quest: .campaign/xapiand/quests/stopword-lists.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fixing the first run's two failures and the per-request schema cost (head eae0478d9). The lists, schema
recording, check and docs are in; tests pass (261119-093906-b28504), the fingerprint mutant fails them
(261119-093843-4ea7ac, branch deleted). Waiting on the strict probes and the suite.

## Next

1. Read 261119-093906-670fdd (schema check, cold and warm) and 261119-093907-83563b (A5 after); write both into
   docs/perf/stopword-lists.md (medians per mode: `python3 -I ~/.scratch/xapiand/stopword-lists/med.py LOG`).
2. Read the suite 261119-093907-ca11c3 (A7); fix what it shows.
3. Fill Result; State ready for review.

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
