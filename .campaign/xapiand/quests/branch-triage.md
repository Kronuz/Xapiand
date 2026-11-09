# branch-triage: Remote branch cleanup triage

Risk: low. History: this landed before the campaign, under the earlier campaign skill, at strata:18febff (2026-10-30); its ledger `docs/status/branch-triage-2026-10-30.md` is the record. Its machine state is in `state.json`: `campaign status branch-triage`.

## Outcome

The triage classifies remote branches across strata, prospect, http, and termdict and proposes cleanup actions. Proposed deletions retain lightweight archive tags; worktree branches are kept until their worktrees are removed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] No explicit criteria; from the Done list: 106 documents were landed in strata, plus the outline source map: `docs/status/branch-triage-2026-10-30.md`
- A2 [artifact] No explicit criteria; from the Done list: prospect:2f65633 and termdict:dc42a4f landed documents: `docs/status/branch-triage-2026-10-30.md`
- A3 [artifact] No explicit criteria; from the Done list: modified docs on branches were not landed where main had newer or unrelated versions: `docs/status/branch-triage-2026-10-30.md`
- A4 [artifact] No explicit criteria; from the Done list: duplicate inverter notes were not landed in strata because prospect main already held them: `docs/status/branch-triage-2026-10-30.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Owner requested triage on 2026-10-30.
- Nothing was deleted or force-pushed; each proposed deletion has an `archive/<branch>` lightweight tag.
- Worktree branches remain until the worktree is removed.
- Risk low: Inventory and recommendations only; no branches were deleted or force-pushed (the lead's mapping).

## May land early

None.
