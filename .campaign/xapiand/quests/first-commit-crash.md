# first-commit-crash: A crash in a fresh index's first commit leaves an index that opens

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status first-commit-crash`.

## Outcome

A power cut or process crash at any point of a fresh (deferred) index's first commit leaves an
index that opens: either empty (the commit never happened) or with the committed documents. Today
strata main refuses 40 of 90 such cuts with `corrupt_index: no manifest log` (probe
261123-043512-f46a17 at strata:00eb018d: `seg-1.sx` on disk, no manifest log), while every cut of
a second commit recovers (0 of 63 refused). `commit()` runs `flush()` (index.hh:1286), which
writes `seg-N`, before `commit_files()` (:1290) rolls over to the first manifest, and
`uncreated()` (:5731-5736) admits only `manifest-1.tmp`, so `recover()` throws at :5830. With
millions of indexes per host, every new index is exposed. group-heal fixed the same shape on
install's path only (strata:a1e786f4, its F2).

## Acceptance criteria

- A1 Every cut of a fresh index's first commit with documents, as a process crash and as power
  loss over at least 8 seeds, reopens empty or committed, never refused and never partial: a test
  in `test/first_commit*` or `test/lifecycle_test.cc`, modeled on the probe in
  `.scratch/xapiand/explore-deferred-first-commit/` (Xapiand home), run by the suite.
- A2 The same test fails at strata main without the fix, after a complete build, naming the
  refused cut (a fails-without job).
- A3 A second commit's sweep (the control) still recovers every cut to the previous or committed
  state, and the existing lifecycle and manifest-log crash tests pass unchanged.
- A4 A mutant per moving part of the fix (for example, recovery that keeps an orphan segment, or
  an order that writes a segment before the first manifest exists) is killed by name.
- A5 [artifact] `docs/mechanisms/manifest-log-file.md` states how a first commit is made crash
  safe and what recovery does with files a manifest never named.

## Contracts

None changes: the on-disk format and the open API stay as they are. Recovery may delete files no
manifest names, and only in a directory that has no manifest log.

## Decisions

- Lead, 2026-11-23: fix the cause, not the error. The contributor decides between ordering (the
  first manifest exists before any segment) and recovery (a directory with no manifest log is
  uncreated, whatever segments it holds), and records why. Whichever it is must not let recovery
  delete data a committed manifest names.
- Lead, 2026-11-23: lands after turn-log-parity, which holds index.hh. index-restructure moves
  index.hh's member bodies by tool, so it regenerates its moves after this lands.

## May land early

Nothing: one fix and its tests.

## Tasks

None: one contributor (implementation, strong, high).
