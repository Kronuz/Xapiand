# stash-claim-hint-publish: Claim hint published once per chunk after the fill (stash#1)

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at stash:d2e8e59 (2026-11-02); its ledger `docs/status/claim-hint-publish.md` in Kronuz/stash is the record. The quest's repository is `../stash`. Its machine state is in `state.json`: `campaign status stash-claim-hint-publish`.

## Outcome

The claim hint is published by one release store per chunk after the fill, with no threshold: depth-64 p99 is back at START on the i9 (0.989 / 0.995 against A/A 0.993 / 0.999) and the deep rows keep their win.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] A strict interleaved A/B (START, main, START again) on the four benches with p50 and p99 per row (`docs/perf/claim-hint-publish.md`): `docs/status/claim-hint-publish.md`
- A2 [artifact] A publish whose depth-64 p99 is at START on the i9, with TSan and ASan clean, tests, mutants, and chain-walk rows within 3% of main: `docs/status/claim-hint-publish.md`
- A3 [artifact] The M4 Pro and Linux measured once: `docs/status/claim-hint-publish.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Five experiments recorded; four losers removed.
- Risk normal: publication ordering in a lock-free structure (the auditor's mapping).

## May land early

None.
