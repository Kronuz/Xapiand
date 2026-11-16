# reclaim-backlog: Reclaim obsolete merge files incrementally

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:b3dcc48 (2026-10-21); its ledger `docs/status/reclaim-backlog.md` is the record. Its machine state is in `state.json`: `campaign status reclaim-backlog`.

## Outcome

Merge publication queues obsolete bytes for reclamation after retention checks. Reclaim work is capped per publish, while settle drains the queue by default.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Count bytes obsoleted by installed or discarded merge outputs; after `retain()`, reclaim in 16 MiB steps: `docs/status/reclaim-backlog.md`
- A2 [artifact] (no explicit criteria; from the Done list) Keep committed, unflushed, and pending-merge source files out of the reclaim queue: `docs/status/reclaim-backlog.md`
- A3 [artifact] (no explicit criteria; from the Done list) Add settle-time reclaim and `KV_SEGREPORT` diagnostics: `docs/status/reclaim-backlog.md`
- A4 [artifact] (no explicit criteria; from the Done list) Test 80 rounds of merges, commits, and a pending merge whose source leaves the list: `docs/status/reclaim-backlog.md`
- A5 [artifact] (no explicit criteria; from the Done list) Ensure mutants for reclaim-before-retain, direct source queueing, and dropping pending sources fail tests: `docs/status/reclaim-backlog.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- CR2: cap each publish at `reclaim_cap` (64 MiB); pay debt across later calls, commits, or explicit reclaim.
- CR5: scan and free views before `retain()`; publish reader-visible state before reclaim.
- CR3: accept that owner-thread deletion of about 600 MB makes 64 KiB ycsb_a run phase 9 to 17% below main; executor deletion is separate work.
- Risk high: Changes file deletion, retention, and reclaim behavior (the lead's mapping).

## May land early

None.
