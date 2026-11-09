# phase7c-gate-prep: Prepare master for X7 gate

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:11e3d82a4 (2026-11-02); its ledger `docs/status/phase7c-gate-prep.md` is the record. Its machine state is in `state.json`: `campaign status phase7c-gate-prep`.

## Outcome

Untrusted index paths are bounded and validated, including absolute paths and malformed names. Node-level routes work without an index endpoint, WAL reads run in a query chore, and owed async rows are classified or removed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Criterion 1: owed async rows are all allow or deleted; `--strict --owed` exits 0: `docs/status/phase7c-gate-prep.md`
- A2 [artifact] Criterion 2: absolute index paths are refused with HTTP 400 or CLI usage error: `docs/status/phase7c-gate-prep.md`
- A3 [artifact] Criterion 3: security-review fixes, including SR1 and root routes, are applied: `docs/status/phase7c-gate-prep.md`
- A4 [artifact] Criterion 4: dead includes are removed: `docs/status/phase7c-gate-prep.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- D4, D5, D6, D7 allowances recorded with their rationale.
- `Endpoint` rejects paths not strictly inside its working directory.
- SR1 root-cause fix belongs in `fs`; lead to push and pin it.
- Risk high: Changes secure untrusted paths and lifecycle-sensitive execution (the lead's mapping).

## May land early

None.
