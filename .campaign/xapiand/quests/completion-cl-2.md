# completion-cl-2: One owner-lock API (CL-2)

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at completion:ca0889d (2026-10-20); its ledger `docs/status/cl-2.md` in Kronuz/completion is the record. The quest's repository is `../completion`. Its machine state is in `state.json`: `campaign status completion-cl-2`.

## Outcome

completion has one owner-lock API, `acquire_owner(OwnerLockOptions)` with file, directory and both modes and legacy presets forwarding to it, closing CL-2's correctness defect (two legacy lock protocols that do not exclude each other).

## Acceptance criteria

Carried from the ledger's Done and review fixes, each shown by the ledger as an artifact.

- A1 [artifact] `acquire_owner(OwnerLockOptions)` and suite `completion_owner_lock`; six mutation probes fail it (the reviewer: 12 of 14 killed, 2 unobservable): `docs/status/cl-2.md`
- A2 [artifact] CI run 39380284199 green on all 8 jobs (17 excluding pairings, 8 legacy controls per topology): `docs/status/cl-2.md`
- A3 [artifact] Docs: README, ARCHITECTURE, AGENTS, PLATFORM_FINDINGS section 9: `docs/status/cl-2.md`
- A4 [artifact] C4: the catalog entry (cpp-libraries:5d41c2d): `docs/status/cl-2.md`
- A5 [artifact] Detent replay 167/167 and journal 8/8 against the branch: `docs/status/cl-2.md`
- A6 [artifact] Review fixes CL2-1 to CL2-7, Debug, ASan+UBSan and TSan 16/16: `docs/status/cl-2.md`
- A7 [artifact] Consumer migrations: Detent to `both`, journal `MemoryIO`, strata `faultfs`: `docs/status/cl-2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Spec: Detent's `docs/CONSOLIDATION_RESPONSE.md` ("CL-2: one ownership protocol").
- Then deprecate the bool form (after the migrations).
- Risk high: lock exclusion between processes on one directory (the auditor's mapping).

## May land early

None.
