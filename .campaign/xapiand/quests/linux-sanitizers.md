# linux-sanitizers: Fix Linux sanitizer failures

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at Xapiand:4bb979da8 (2026-11-03); its ledger `docs/status/linux-sanitizers.md` is the record. Its machine state is in `state.json`: `campaign status linux-sanitizers`.

## Outcome

Index chores remain counted until their awaiting task resumes, preventing teardown from mistaking active work for idle. Path-boundary testing uses absolute paths, TSan exception ownership is stabilized, and shutdown fallback blocking and test diagnostics are corrected.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] Items 1 to 5, `test_node` segfault and shutdown-drain debug abort closed with before/after evidence: `docs/status/linux-sanitizers.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Leave LZ4 v1.8.1 unchanged unless clang-18 becomes a gate.
- Push traceback `64cc1052d4fb99fcf523d6ca1b4ad54b99ac63e0` before the Xapiand branch can fetch it.
- Risk high: Fixes concurrency, shutdown, and untrusted execution paths (the lead's mapping).

## May land early

None.
