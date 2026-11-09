# recovery-tooling: Recovery tooling, the `strata` command-line tool

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status recovery-tooling`.

## Outcome

An operator can heal or recover a damaged strata index with one `strata` command-line tool: inspect a
manifest's segments, sections and checksums with the exact byte range that failed; verify a whole
index offline or against a live one, read-only; salvage every intact document out of a damaged segment
and report the lost ones by docid and key; rebuild a clean index from salvaged documents and derived
structures from stored records; and restore from a replica, the journal or a checkpoint when one
exists. Across corruption of every file class at every offset, the tool never crashes, never writes
into the damaged index, and recovers every document whose stored record is intact.

## Acceptance criteria

From strata `docs/PLAN.md`, "Recovery tooling (after the engine is complete)" (owner, 2026-10-18).

- A1 Inspect: a manifest's segments and sections, and every checksum, with the exact byte range that failed: tests in jobs
- A2 Verify: a whole index, offline or against a live one, read-only: tests in jobs
- A3 Salvage: every intact document out of a damaged segment (stored records, keys, versions), and the lost ones reported by docid and key: tests in jobs
- A4 Rebuild: salvaged documents re-indexed into a new, clean index; derived structures (postings, term dictionary, norms) rebuilt from stored records; only what cannot be recovered dropped: tests in jobs
- A5 Restore from elsewhere: a replica, the journal or a checkpoint, when one exists: tests in jobs
- A6 Gate: corruption of every file class at every offset in a fuzzed sweep; the tool never crashes, never writes into the damaged index, and recovers every document whose stored record is intact: fuzz and sweep jobs under ASan and UBSan
- A7 [artifact] How it works and how an operator uses it: `docs/mechanisms/recovery-tool.md`

## Contracts

Reads every strata and prospect format the release ships (`check()`, `heal()` and `recover --to` are
the engine's own detection and fallback today); writes only new indexes.

## Decisions

- After the engine is complete (owner, 2026-10-18, D28): blocked until then; first named "Track H:
  inspection and recovery tooling" (strata:002737fc), renamed (strata:199770f9).
- The engine already detects damage (checksums, `check()`, `heal()` reporting a segment) and replicas
  repair from other voters; this is the operator's tool (strata `docs/design/media-faults.md`).

## May land early

Each subcommand with its tests.
