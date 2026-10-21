# Extraction ledger

Every generic piece that leaves Xapiand for a Kronuz library (the "foundations") gets one row here,
recording how much Xapiand shrank and what proves nothing was lost. The doctrine is in
`~/code/Kronuz/KRONUZ_REFACTORING_DIRECTIVES.md`; the backlog of candidates is
`~/code/Kronuz/memory/kronuz-extraction-backlog.md`.

## How to add a row

- **Piece:** what moved, in Xapiand's terms.
- **Library:** the `Kronuz/*` library it moved to (existing, per the cpp-libraries catalog, or new),
  and its commit.
- **Removed from Xapiand:** files and lines deleted (`git diff --stat` of the removal commit), and
  the lines of glue added back (the seam, the trace header). Net reduction = deleted − glue.
- **Reduced how:** the kind of reduction: whole subsystem moved, duplicate replaced by an existing
  library, vendored copy dropped, or code made unnecessary by a library feature.
- **Kept in Xapiand:** the irreducible domain glue that stays, and why.
- **Proof:** unit tests in the library, Xapiand's E2E suite (docs as tests) and cluster nets
  before and after, and a benchmark when a hot path moved.
- **Xapiand commit:** the commit that removed it.

Measure lines with the same tool every time (`git diff --stat` on `src/` and `CMakeLists.txt`), and
count tests separately.

## Rows

| Date | Piece | Library (commit) | Deleted | Glue added | Net | Reduced how | Kept in Xapiand | Proof | Xapiand commit |
|---|---|---|---:|---:|---:|---|---|---|---|
| 2026-10-20 | Index-time text analysis (`GenerateTerms::text`, `getGeneratorStopStrategy`/`getGeneratorStemStrategy`, the phf `SimpleStopper`) | `Kronuz/analysis` (`a0825b1`), `Kronuz/snowball` (`570a0b7`), `Kronuz/unicode` (`c93026c`) | 75 | 180 | -105 (grew) | Code made unnecessary by a library: the default path's Xapian term generator, stemmer and stopper, and the hash-only stopper, replaced by `analysis::term_generator`, `snowball::stemmer` and `analysis::stopper` | The `cjk_words` path under `USE_ICU` (Xapian word breaks; analysis has none); `Xapian::Stem` and the `Xapian::Stopper` adapter for the query side until `queryparser` replaces `Xapian::QueryParser` (S4); the Xapian `Document` sink | E2E docs suite on 335 requests: 570 of 570 assertions pass before and after (also under ASan and UBSan, no reports in the changed code), bodies differ only in 2 volatile fields on both runs; `harness/terms_diff.py` (130 configurations) and `harness/vocab_diff.py` (18 languages) equal except Danish, Italian and Norwegian apostrophe stems (newer Snowball) and German stop words (the old stopper never built from a duplicate in `de.txt`, now fixed); a mutant of the stop mapping changes 168 documents | `fe9307227` |
| 2026-10-20 | Word-break segmentation for `cjk_words` (`text_word_breaks` in `GenerateTerms::text`) | none yet (`Kronuz/analysis` has no word-break segmentation) | 0 | 0 | 0 | Stays on `Xapian::TermGenerator` under `USE_ICU` until `analysis` gains word-break segmentation; then the path, its two strategy mappers and the `Xapian::TermGenerator` references go | The whole path | E2E asserts only that the flag defaults to false; no differential covers it | n/a |
| 2026-10-21 | Engine seam, document half (X2 half A): `Document`, ids and counts, open actions, error kinds, `DocView` and the Xapian conversion behind `src/engine/` | none (Xapiand-owned seam, `src/engine/`; Xapian stays the first implementation) | 164 | 786 | +622 (the seam: 572 lines in `src/engine/` plus call-site glue) | Seam, not extraction: `Xapian::Document` and its id types leave `schema.cc`, `utils.{h,cc}`, `flags.h` and the write path; seam count 56 -> 52 files, 1557 -> 1418 `Xapian::` lines | Conversion at the database call, the Xapian-typed `Shard` writes the Xapian remote server calls, query side (X3), reads (X4) | E2E 576/576 and `e2e_diff` GREEN against the X1 capture; analysis differentials byte-identical; `wal_recovery_check`, `cluster_check 2` and `3` pass; 840-check differential unit test against Xapian documents, five mutants caught | `7eae6786c` |
| 2026-10-21 | Engine seam, shard half (X2 half B): the write and lifecycle slice of `Shard` behind `engine::Shard` (`task::task<T>` per D13), with the three copies of the version dance collapsed into one Xapian implementation, shard opening, and retry ladders on error kinds | none (Xapiand-owned seam; `Kronuz/task` `aa447e7` for the tasks) | 624 | 1253 | +629 (new: 791 lines in `src/engine/` for `engine::Shard`, its Xapian implementation and the error types; `shard.cc` 2163 -> 1940) | Duplicate replaced by one: `Shard::{add,replace,replace_by_key}_document` each carried a ~100-line version dance (stored-version lookup, check, bump, bookkeeping), now `check_version`/`stamp` once in the engine; nine identical catch ladders became `retry_or_throw` | Xapian-typed `Shard` overloads for the Xapian remote server, the `finalise_write` data hook (Xapiand's `Data` and numeric ids), reads through `db()` (X4), read-path catches | E2E 576/576, `e2e_diff` GREEN with the pool off and on; analysis differentials byte-identical; `wal_recovery_check`, `cluster_check 2` and `3`; 52-check shard test; five mutants caught; seam metric 52 files, `Xapian::` lines in shard.cc 215 -> 55 | `3be508883` |

## Backfill

Earlier extractions (msgpack-c, fmt, RapidJSON, LZ4, libev, TCLAP, cppcodec, ChaiScript, libyaml,
reactor, cluster, http, logger, and the roughly forty utility libraries) predate this ledger. Backfill
their rows from git history when a session touches them; until then the backlog file holds their
record.
