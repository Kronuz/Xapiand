# cold-tier-step2-item5b: Catalog host interface and destroy-safe pins

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:5d742b2 (2026-11-04); its ledger `docs/status/cold-tier-step2.md` is the record. Its machine state is in `state.json`: `campaign status cold-tier-step2-item5b`.

## Outcome

The catalog's host interface serves X-POOL: create and resident intents, in-place payloads, reopen drains, snapshots, fenced replacement and counted host files. Waiters live on the heap, so a frame destroyed while it waits unlinks its waiter and releases its pin, and a cancel token unlinks a waiting pin.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] S10 Create and resident pins honor absent and negative entries and handle opening slots: `docs/status/cold-tier-step2.md`
- A2 [artifact] S11 In-place payload construction exposes completed state only after the open step; failures destroy resources in order: `docs/status/cold-tier-step2.md`
- A3 [artifact] S12 Reopen drains holders and serves waiters from a fresh index: `docs/status/cold-tier-step2.md`
- A4 [artifact] S13 Fenced slots replace correctly with bounded retry and backoff: `docs/status/cold-tier-step2.md`
- A5 [artifact] S14 A finished merge's check-in allows demotion; merge output is committed when required: `docs/status/cold-tier-step2.md`
- A6 [artifact] S15 Snapshot includes all non-free slots; flush commits eligible entries: `docs/status/cold-tier-step2.md`
- A7 [artifact] S16 Host file reservations count and fail before kernel calls at caps: `docs/status/cold-tier-step2.md`
- A8 [artifact] C14 Wake gate oracle remains clean; C17 debug-checks calls without a pin: `docs/status/cold-tier-step2.md`
- A9 [artifact] Text for cold-tier design revision 8, mechanism note and limits rows: `docs/status/cold-tier-step2.md`
- A10 [artifact] No hot-path regression: strict-hold pin/unpin smoke A/B against item 5: `docs/status/cold-tier-step2.md`
- A11 [artifact] PF1 Cancellation and abandonment never touch a destroyed catalog after orphaning: `docs/status/cold-tier-step2.md`
- A12 [artifact] PF2 A frame dies only after its scheduler closes or while no delivery can run: `docs/status/cold-tier-step2.md`
- A13 [artifact] PF3 `stats().cancels` counts waits ended by a token, not tokens requested before waiting: `docs/status/cold-tier-step2.md`
- A14 [artifact] PF5 Design vs code recorded; a requested token still starts an open or wake through `try_pin`: `docs/status/cold-tier-step2.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- A task's second pin of a key does not reuse its first; self-wait remains caller-owned.
- `~catalog` orphans leftover waiters.
- Requested tokens may still start opens for cold keys.
- `~catalog` orphans leftover waiters (PF1).
- Risk high: Changes concurrent waiter ownership, cancellation and pin accounting (the lead's mapping).

## May land early

None.
