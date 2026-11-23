# uinteger-radix-family: uinteger_t's output helpers leave radix_decode.hh

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status uinteger-radix-family`.

## Outcome

`uinteger_t/radix_decode.hh` holds only parsing (text to number). The output-side members it still
holds after uinteger-restructure (`_store_be`, `_store_bytes`, `_digits_append`, `_digits_radix`)
move, unchanged, to the family header they belong with: `radix_encode.hh` for number-to-text, or
`digits.hh` for the digit buffer, whichever each member serves. Follow-up F1 of uinteger-restructure's
review (hunt.md).

## Acceptance criteria

- A1 The suite passes: the `build-test` check, a job tagged `--for A1`
- A2 The move changes no code: the `members` check (`split_check.py`) passes, its warnings answered in
  the journal, and a job tagged `--for A2`
- A3 [artifact] `docs/source-layout.md` (or the layout note uinteger-restructure wrote) names each
  member's new home
- A4 [manual] The lead reads the diff and agrees each header is one family

## Contracts

None: `uinteger_t.hh` stays the umbrella; no include a user writes changes.

## Decisions

- Lead, 2026-11-23: a tool-made move (cpp-libraries `tools/outline.py` map change or a plain cut and
  paste of whole definitions), one commit. If a member serves both directions, it stays and the
  journal says why.

## May land early

Nothing: one move.

## Tasks

None: one contributor (maintenance, economical, medium).
