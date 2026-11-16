# strata-pins-2026-11: Update library dependency pins

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:f2d7626 (2026-11-02); its ledger `docs/status/pins-2026-11.md` is the record. Its machine state is in `state.json`: `campaign status strata-pins-2026-11`.

## Outcome

Strata's CMake pins move to the library heads, with the fusefilter format-changing move held separately. The held move changes filter bytes and requires a pre-freeze version bump.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] No explicit criteria; from the Done list: pins move to the listed library heads in CMakeLists.txt: `docs/status/pins-2026-11.md`
- A2 [artifact] No explicit criteria; from the Done list: fusefilter move is isolated in a branch with pre-freeze version bump and repinned fixtures/digests: `docs/status/pins-2026-11.md`
- A3 [artifact] No explicit criteria; from the Done list: resolved dependency trees are compared with library heads and no prohibited pin range is used: `docs/status/pins-2026-11.md`
- A4 [artifact] No explicit criteria; from the Done list: release ctest and hot-path comparison are run: `docs/status/pins-2026-11.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Keep fusefilter 0665ee0; 08686a5 changes filter bytes and requires a format bump.
- Fusefilter bump is blocked pending architect or owner ruling.
- Risk high: Fusefilter pin changes pre-freeze format bytes and needs a format ruling (the lead's mapping).

## May land early

None.
