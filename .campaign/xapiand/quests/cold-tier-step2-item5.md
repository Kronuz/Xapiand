# cold-tier-step2-item5: Catalog sweep and shutdown

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:676285d (2026-11-04); its ledger `docs/status/cold-tier-step2.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step2-item5`.

## Outcome

The sweep demotes, reclaims and closes eligible slots, coalescing cap-crossing notifications and retrying work. Shutdown commits and closes slots, reports failures and waits for outstanding work.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S7.1 Demotion only from `{0, open}`: `docs/status/cold-tier-step2.md`
- A2 [artifact] S7.2 Close only from warm, with zero maps and `close_ready()`: `docs/status/cold-tier-step2.md`
- A3 [artifact] S7.3 Queued reclaim posts and closes on a later pass: `docs/status/cold-tier-step2.md`
- A4 [artifact] S7.4 `reclaim(0)` runs under demoting: `docs/status/cold-tier-step2.md`
- A5 [artifact] S7.5 A held view keeps a slot closing without blocking a thread: `docs/status/cold-tier-step2.md`
- A6 [artifact] S7.6 A crossing posts one coalesced pass that reposts while over the soft cap: `docs/status/cold-tier-step2.md`
- A7 [artifact] S7.7 Shutdown commits uncommitted slots, closes every slot and names failed commits: `docs/status/cold-tier-step2.md`
- A8 [artifact] S7.8 `H` is destroyed after the close chore returns: `docs/status/cold-tier-step2.md`
- A9 [artifact] C15.1 Destroying with a pin held asserts in debug builds: `docs/status/cold-tier-step2.md`
- A10 [artifact] C15.2 Destroying open slots leaks their IO and lock rather than destroying them: `docs/status/cold-tier-step2.md`
- A11 [artifact] C15.3 Reused slots reset failures, error, retry/probe time and last use: `docs/status/cold-tier-step2.md`
- A12 [artifact] C15.4 A stuck close remains closing, is reported and counted, and is retried: `docs/status/cold-tier-step2.md`
- A13 [artifact] C15.5 Throwing close ends at prior state and is counted: `docs/status/cold-tier-step2.md`
- A14 [artifact] FREE Expired failed entries are erased; a pin arriving first reopens: `docs/status/cold-tier-step2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- A pass that posts nothing does not repost; the 1 Hz timer retries.
- Shutdown closes after commit without `close_ready()` and does not post reclaim.
- Destruction with open slots leaks their resources; unfinished shutdown must be abandoned by exiting.
- Risk high: Adds concurrent eviction, reclaim, close retries and shutdown (the lead's mapping).

## May land early

None.
