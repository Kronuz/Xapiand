> Status: CURRENT (2026-11-02) the X-POOL developer's ledger (phase 7c: Xapiand adopts strata's cold-tier catalog); local branch `phase7c/x-pool` from master `Xapiand:88892b4cb`, not pushed.

# phase7c X-POOL (build): the registry becomes the catalog's payload

Branch `phase7c/x-pool` in `Xapiand-wt/x-pool`. Spec: `strata/docs/design/cold-tier.md` revision 7
(`design/cold-tier`, strata:5723f3d), "Steps", "Xapiand adopts it". Design: `docs/design/x-pool.md`.
Builds against strata `phase/cold-tier-step2-item4` (strata:1b0c382) with
`-DFETCHCONTENT_SOURCE_DIR_STRATA=<a strata worktree>`; item 5 is read at strata:a748805.

## State

Design note written (D1 to D12, gate rows XP1 to XP7, steps 1 to 4). Stopped for the lead's skim:
the catalog's host interface cannot host Xapiand's open (gaps F1 to F6 in the note). No code yet.

## Done criteria

- [x] `docs/design/x-pool.md`: numbered decisions, gate rows, the gaps with recommendations.
- [ ] Step 1, the seam: `IndexPin` over today's `refs`, `lock_shard` awaited, pin-before-view check (XP1).
- [ ] Step 2, the registry as `H` (after F1 to F3): XP1, XP2.
- [ ] Step 3, commit chore, idle close gone, shutdown, housekeeping (after item 5 and F4 to F6): XP3, XP4.
- [ ] Step 4, `map_owed`, the `unleased_maps()` assertion, the budget and CT-R39's clamp: XP5.
- [ ] Proof: TSan and ASan+UBSan builds, a mutant per ordering choice, `async_check --strict --owed
  --extern strata=<pin>` at 0, docs E2E (`npx newman@6.2.3`), `graceful_shutdown_check.sh`,
  `path_boundary_check.sh` (XP6, XP7).
- [ ] After wide-search lands: its `search_batch` pins through `IndexPin`, K clamped to `soft_open`.

Host rule (the lead, 2026-11-02): the TSan and ASan+UBSan batches run on Linux through strata's
`tools/vm/aws-linux.sh` (shared 32-core lease, the branch shipped as a git bundle); only macOS builds
run locally.

## Done

- Design note and this ledger (this commit).

## Next

1. On the lead's go: step 1 (the seam), which needs none of F1 to F6.
2. After the architect rules F1 to F3 and strata builds them: step 2.

## Blocked on

- **The architect, on the catalog's host interface** (strata, item 4/5 or an item of its own):
  F1 create intent per pin (negative entries make create-on-write fail up to 10 s), F2 payload before
  the index and per-index options, F3 an open-time host step for the WAL replay, F4 reopen in place on
  request (fenced, restore), F5 host descriptors counted on the slot, F6 host work per open index.
  Recommendations in `docs/design/x-pool.md`, "Gaps".
