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

## Backfill

Earlier extractions (msgpack-c, fmt, RapidJSON, LZ4, libev, TCLAP, cppcodec, ChaiScript, libyaml,
reactor, cluster, http, logger, and the roughly forty utility libraries) predate this ledger. Backfill
their rows from git history when a session touches them; until then the backlog file holds their
record.
