# idle-index: Idle index cost landscape

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:1a04bc3 (2026-10-27); its ledger `docs/status/idle-index.md` is the record. Its machine state is in `state.json`: `campaign status idle-index`.

## Outcome

The `idle_index` harness measures build, open, idle, query, and close costs, and `idle_index_count` reports allocation sites. The report records platform limits, per-index heap use, and candidate reductions.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Harness `idle_index` measures build/open, bare and shared, idle window, query, and close; `idle_index_count` measures allocation sites: `docs/status/idle-index.md`
- A2 [artifact] macOS matrix covers 0/1/10/100/1,000 documents at 1k, 10k, 100k open; reports 126,507 and 252,492 open indexes: `docs/status/idle-index.md`
- A3 [artifact] Linux VM covers the same matrix, cold and warm opens, 300,000 open, map-count limit, and open syscall trace: `docs/status/idle-index.md`
- A4 [artifact] Heap breakdown, limits table, top three cuts, and field comparison are reported: `docs/status/idle-index.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- The closed-handle tier is a host-policy decision, parked by the lead (small-indexes F3).
- Risk low: Research and measurement; no engine code change (the lead's mapping).

## May land early

None.
