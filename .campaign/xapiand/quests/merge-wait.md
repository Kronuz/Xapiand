# merge-wait: Remove merge budget waiting

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at strata:6e13416 (2026-10-20); its ledger `docs/status/merge-wait.md` is the record. Its machine state is in `state.json`: `campaign status merge-wait`.

## Outcome

Merges charge working memory as they build rather than waiting for budget pressure to clear. The wait option is removed, allowing flush pressure to respond sooner.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Merge wait removed and options/environment updated: `docs/status/merge-wait.md`
- A2 [artifact] Tests verify below-capacity behavior, memory accounting, and absence of sleeps: `docs/status/merge-wait.md`
- A3 [artifact] RelWithDebInfo, TSan, and ASan+UBSan suites pass: `docs/status/merge-wait.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead, 2026-10-20: fix the merge admission and wait path only; merge ordering and key merge are out of scope.
- A separate merge share remains a possible node-level budget API, not part of this fix.
- Risk normal: fixes merge admission stalls (the lead's mapping).

## May land early

None.
