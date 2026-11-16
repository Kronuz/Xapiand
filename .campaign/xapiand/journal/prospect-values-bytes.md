# prospect-values-bytes: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-values-bytes.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:eee749f on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/values-bytes.md`: "`prospect:ca67b32` the plugin: `values.hh` (part, stage 1, reader, check, digest records)," |
| A2 | `docs/status/values-bytes.md`: "`values_test` compares a merge's values section with the direct flush's of the live documents byte for byte" |
| A3 | `docs/status/values-bytes.md`: "Revert check: with `(void)entries()` back in the constructor `values_test` fails at `verified_regions() == 0`" |
| A4 | `docs/status/values-bytes.md`: "I merge numbers by the old ordinal. All nine fail." |
| A5 | `docs/status/values-bytes.md`: "background_merge, fuzz_values_replay clean (own tree, `.scratch/build-tsan`)" |
| A6 | `docs/status/values-bytes.md`: "12,228 runs in 10 minutes, 0 findings, corpus of 275 inputs checked in and replayed" |
| A7 | open: not run; the lead schedules the long run in `docs/status/values-bytes.md` |
| A8 | `docs/mechanisms/values.md`: "how the values plugin works; decisions and evidence in" |
| A9 | `docs/status/values-bytes.md`: "`prospect:8a1b148`: all 8 jobs green, linux-tsan in 25 minutes" |
| A10 | `docs/status/values-bytes.md`: "branch 1.174 1.184 1.175 1.201 1.164 1.205 s (mean 1.184). Neutral within the noise." |

## Result

Landed at prospect:eee749f on 2026-10-27.

- Review: `docs/reviews/values-bytes-security.md` -- APPROVE WITH CONDITIONS.
- Docs: `docs/mechanisms/values.md`.
- Left open: A7.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
