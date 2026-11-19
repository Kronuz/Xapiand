> Status: CURRENT (2026-11-19) quest/xapiand/stopword-lists (D137); not landed.

# Stop-word lists: compiled in, overridable, fingerprinted

## What

The 18 shipped stop-word lists are part of the binary. A file `<data path>/stopwords/<language>.txt`
replaces a language's list, and a file with another name adds a list. Each list has a 64-bit fingerprint
so an index can tell that the list it was built with is gone or changed. A language with no list is an error
(`UnknownStopWordsError`), never "stop nothing".

## How

```
stopwords/*.txt --generate.py--> src/stopwords/builtin.inc  (blob per language, fingerprint at compile time)
<data path>/stopwords/*.txt --preloadStopWords()--> overrides (std::map, read once, immutable)

getStopWords(lang):  overrides[lang]  else  builtin[lang] (an analysis::stopper built on first use)  else throw
```

1. A list is a blob of its words, sorted by bytes without repeats, each followed by a NUL
   (`src/stopwords/builtin.h`). Its fingerprint is the FNV-1a 64-bit hash of the blob, printed as 16 hex
   digits. A file is hashed the same way after sorting and removing repeats, so the same words give the same
   fingerprint wherever they come from.
2. `src/stopwords/generate.py` writes `builtin.inc` from `stopwords/*.txt`.
   `tests/test_stopword_lists.cc` fails when the two differ (every shipped file against its compiled list, the
   word counts and the fingerprints).
3. `preloadStopWords()` reads the override files once, at startup, so no request reads a file. A file that
   cannot be read, or a directory with a list's name, throws: a quiet fallback to the shipped list is what D137 forbids. With no
   stopwords directory nothing is read.
4. A shipped list becomes an `analysis::stopper` (the analysis library's `unordered_set`) the first time its
   language is asked for, once per process (`std::call_once`); later lookups read it.
5. `checkStopWords(index, language, recorded)` throws `StopWordsMismatchError` naming the index, the list and
   both fingerprints when the list is missing or hashes differently.
6. The query side turns `UnknownStopWordsError` into `InvalidTextConfigError` (`src/engine/text.cc`) and the
   index side into an invalid `engine::Error` (`src/multivalue/generate_terms.cc`).

7. A text field with a language stores `_stop_fingerprint` beside `_language` when it is created
   (`validate_required_data`, `src/database/schema.cc`). `DatabaseHandler::get_schema` calls
   `Schema::check_stop_words(index, adopt)` on every schema it hands out: each field with a language is
   checked with `checkStopWords`, and a mismatch is a `StopWordsMismatchError` that names the index (the
   first shard's path without the shard suffix) and answers `409`: the request and the index are fine, the node's
   list is not the one the index was built with. `DatabaseHandler::get_mset` also checks every other distinct index
   of the search with `Schema::check_cached_stop_words` on the schema the cache holds (no adoption); an index that
   does not load is left to the search. An index found to agree is remembered by path for the process (the lists are
   fixed and a schema written here records the current ones), so a wide search over a schema pool smaller than its
   index count loads each schema once, not on every search (evicting and reloading 2,000 schemas through 100
   slots on each search is what the uncached check cost). A field with a language and
   no record (an index from before the fingerprints) adopts the current list's fingerprint in the mutable
   schema when the handler is writable, so the next schema write persists it; one info line names the index
   and the list, once per process. `dispatch_readable` (behind `get_full(true)`, the `_schema` of info and
   search responses) hides the property: it is derived from the list, not a setting a client sends back.

The install copies `stopwords/` to `share/doc/xapiand/stopwords`, a directory nothing reads. A build run from
the source tree (`build/bin/xapiand`) finds the repository root as its data path and reads the 18 files there as
overrides. They hold the shipped words, so their fingerprints match the compiled-in ones and no index is
refused.

## Why not a perfect hash

constexpr-phf fits (a 781-key set compiles in about 0.7 s), but every consumer takes
`const analysis::stopper*`, so any compiled set is copied into that `unordered_set` at first use; the PHF would
sit behind the copy unused. Numbers: `docs/perf/stopword-lists.md`.

## Cost

`check_stop_words` walks the schema's fields once per schema version and thread: a schema that passed with
nothing to adopt is remembered by identity (a `thread_local` map of weak pointers, at most 4096 entries), and
the next request that gets the same schema pays one lookup. A schema with fields still to adopt, or one
modified in place, is walked each time. Numbers: `docs/perf/stopword-lists.md`.
