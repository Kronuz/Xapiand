# uinteger-radix-family: journal

State: waiting on jobs 261123-102259-f52610 (build-test, A1) 261123-102259-7b84f2 (members, A2)
Quest: .campaign/xapiand/quests/uinteger-radix-family.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Move done and committed (uinteger_t:88e41ad, pushed to quest branch). Jobs queued at it.

## Next

1. Read the members job log; answer each split_check warning under Rulings.
2. If both pass, ready for review (A4 is the lead's read).

## Blocked on

<What, and who decides. "Nothing" when nothing.>

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261123-102259-f52610 (build-test at uinteger_t:88e41ad) |
| A2 | 261123-102259-7b84f2 (members at uinteger_t:88e41ad): 0 errors; its 3 warnings are the first-line comments of digits.hh, radix_decode.hh and radix_encode.hh, rewritten to name what each holds now (lead, 2026-11-23) |
| A3 | `docs/source-layout.md` (its radix_encode.hh row names _digits_radix and _digits_append; the digits.hh row names _store_be and _store_bytes; the radix_decode.hh row is parsing only) |
| A4 | [manual] the lead (2026-11-23) read uinteger_t:88e41ad: radix_decode.hh holds only input (byte loads, Horner decoder); _digits_append and _digits_radix sit with the encoder they call; _store_be and _store_bytes sit with their one caller, the raw-bytes form in digits.hh. Each header is one family; agreed. |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- `_store_be`, `_store_bytes` -> digits.hh (only caller is the raw-bytes form there); `_digits_append`, `_digits_radix` -> radix_encode.hh (`_digits_radix` calls `_encode_radix`; `_digits_append` is also used by radix_tree.hh, which follows both). `_load_be` stays in radix_decode.hh (input side).
- split_check warnings: not yet read (jobs pending); to answer on resume.
