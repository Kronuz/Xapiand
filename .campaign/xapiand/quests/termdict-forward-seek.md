# termdict-forward-seek: termdict forward seek inside the open block

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at termdict:74d518a (2026-10-21); its ledger `docs/status/forward-seek.md` in Kronuz/termdict is the record. The quest's repository is `../termdict`. Its machine state is in `state.json`: `campaign status termdict-forward-seek`.

## Outcome

`cursor::seek` moves forward inside the open block without the index lookup when the target is past the current key and not past the block's last key (explorer B's change with a last-key test).

## Acceptance criteria

Carried from the ledger's Done list, each shown by the ledger as an artifact.

- A1 [artifact] Explorer B's change (1deaa82) with its heuristic replaced by a last-key test; `block::last_key` decodes only the symbols kept: `docs/status/forward-seek.md`
- A2 [artifact] `check_forward`, corruption-sweep and fuzz forward seeks; four mutants caught: `docs/status/forward-seek.md`
- A3 [artifact] ASan+UBSan clean; 60 s fuzz clean; numbers in `docs/perf/forward-seek.md`: `docs/status/forward-seek.md`
- A4 [artifact] CI on the branch: `docs/status/forward-seek.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Optional follow-up: store the last key in the block (a format change).
- Explorers A and B landed as history (termdict:dc42a4f); their branches stay as records.
- Risk normal: a decoder path in a shared library (the auditor's mapping).

## May land early

None.
