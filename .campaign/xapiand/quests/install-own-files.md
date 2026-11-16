# install-own-files: Reopen installed segments from target files

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:ef48ed0 (2026-10-26); its ledger `docs/status/install-own-files.md` is the record. Its machine state is in `state.json`: `campaign status install-own-files`.

## Outcome

Mapped segments installed from a checkpoint are reopened from the target's own files before publication. This prevents a live source index from reclaiming files still mapped by the target.

## Acceptance criteria

The ledger lists no explicit done criteria; these are its Done items.

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `17e8b3c`: add fix and `install_own_test`: `docs/status/install-own-files.md`
- A2 [artifact] (no explicit criteria; from the Done list) `9cf444d`: correct D5 sentence and add scrubber note: `docs/status/install-own-files.md`
- A3 [artifact] (no explicit criteria; from the Done list) `ff812c6`: add faultfs `damage_writes`: `docs/status/install-own-files.md`
- A4 [artifact] (no explicit criteria; from the Done list) `853dba3`: count EIO in `read_checked`; restrict corruption handling to `corrupt_index` and `corrupt_segment`: `docs/status/install-own-files.md`
- A5 [artifact] (no explicit criteria; from the Done list) C2: allow EMFILE/ENOMEM from reopen to propagate unfenced and permit retry; EIO twice becomes `corrupt_index`: `docs/status/install-own-files.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Architect ruling C1: D5's "verifies each whole" means source checkpoint verification; written copy is checked at reopen and later read/heal/open/scrubber paths.
- C2: reopen runs between guarded regions so host EMFILE/ENOMEM propagates as itself and target remains unfenced.
- Risk high: Changes mapped-file lifetime and install durability behavior (the lead's mapping).

## May land early

None.
