# prospect-pruning-floors: Pruning floors: optional minimum and counted seed floor

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:59f36fe (2026-10-21); its ledger `docs/status/pruning-floors.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-pruning-floors`.

## Outcome

Top-k pruning keeps the optional side's own minimum in `required_optional_scorer` and the seed floor for counted disjunctions, identity-checked against exhaustive search.

## Acceptance criteria

No explicit criteria; from the Done list, each shown by the ledger as an artifact.

- A1 [artifact] Identity (dump counted and uncounted, verify) at 100k and 1M, 0 mismatches: `docs/status/pruning-floors.md`
- A2 [artifact] ctest Release 39/39 and ASan+UBSan 39/39; reverts and two mutants caught: `docs/status/pruning-floors.md`
- A3 [artifact] Timing at 1M reordered, both presets, 12 rounds: both changes hold (`docs/perf/pruning-floors.md`): `docs/status/pruning-floors.md`
- A4 [artifact] Review conditions CR1, CR2, CR3, CR5 folded in: `docs/status/pruning-floors.md`
- A5 [artifact] A 1-3% slowdown on classes neither change reaches: `docs/status/pruning-floors.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Comes from explore/req-opt (bcf0796) and explore/topk-count (3d8da0f); those branches stay unmerged as the explorers' records.
- Risk normal: changes which documents are scored (the auditor's mapping).

## May land early

None.
