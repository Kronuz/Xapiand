# completion-cl-2: journal

State: landed
Quest: .campaign/xapiand/quests/completion-cl-2.md (the lead's; never edit it)

## Doing

Nothing: landed at completion:ca0889d on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cl-2.md`: "Six mutation probes (order, EINTR, file-only fallback, held checks, unwind, EOPNOTSUPP mapping) each fail it" |
| A2 | `docs/status/cl-2.md`: "CI run 39380284199 passed it on freebsd-x86_64, linux-x86_64-gcc" |
| A3 | `docs/status/cl-2.md`: "ARCHITECTURE, AGENTS (traps), PLATFORM_FINDINGS section 9." |
| A4 | `docs/status/cl-2.md`: "cpp-libraries 5d41c2d: the completion catalog entry (C4)." |
| A5 | `docs/status/cl-2.md`: "ctest 166/167, then 167/167 on a full rerun. - journal dbf753f against this branch: Debug, 8/8." |
| A6 | `docs/status/cl-2.md`: "Local, Homebrew clang 23 Debug: Debug, ASan+UBSan and TSan, 16/16 each" |
| A7 | open: Next in the ledger; Detent's part is its RC-1 CL-2 item in `docs/status/cl-2.md` |

## Result

Landed at completion:ca0889d on 2026-10-20. Landed through completion:91d62cb to completion:ca0889d (2026-10-20, the re-check).

- Review: `docs/reviews/cl-2-code.md` -- REJECT on CL2-1, re-check closed CL2-1 to CL2-7 (completion:ca0889d).
- The Detent replay branch `cl-2-detent-replay` is patch-equivalent to main (9 of 9 commits).
- Docs: `docs/PLATFORM_FINDINGS.md`.
- Left open: A7.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
