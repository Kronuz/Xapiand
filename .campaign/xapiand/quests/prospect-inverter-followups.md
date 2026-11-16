# prospect-inverter-followups: Inverter follow-ups (R26 to R33, CR14 to CR20)

Risk: normal. History: this landed before the campaign, under the earlier campaign skill, at prospect:6841837 (2026-10-19); its ledger `docs/status/inverter-followups.md` in Kronuz/prospect is the record. The quest's repository is `../prospect`. Its machine state is in `state.json`: `campaign status prospect-inverter-followups`.

## Outcome

The architect's re-check on main (R26 to R33, C11) and the code review of main (CR14 to CR20) are closed on the inverter: the run cap is private, the tuple buffer asserts its six preconditions, the key goes through `key_of`, 64-bit entry and section lengths, and the ledger states one current state.

## Acceptance criteria

No explicit criteria ("none set yet" in the ledger); from its Done list, each shown by the ledger as an artifact.

- A1 [artifact] R28.1 and CR9: `index_options::max_run_bytes` gone; the run cap is private; byte-identical to main at 20k and 100k: `docs/status/inverter-followups.md`
- A2 [artifact] R27, R28.2-3: debug asserts of all six preconditions; `valid_words` and `add_text` private: `docs/status/inverter-followups.md`
- A3 [artifact] R29, R32.1: the key through `key_of`, hashed by wordwise: `docs/status/inverter-followups.md`
- A4 [artifact] CR15 to CR18 and C11: limits rows, 2^32 - 1 words, capped doubling, small first footprint: `docs/status/inverter-followups.md`
- A5 [artifact] CR17: the lone run and 64-bit lengths each with a test; CR20 one current state: `docs/status/inverter-followups.md`
- A6 [artifact] Every commit builds and passes ctest 35 of 35 before it is committed: `docs/status/inverter-followups.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Lead's rulings recorded with the review (`docs/reviews/inverter-followups.md`, 622b090).
- Risk normal: internal limits and assertions of the segment writer (the auditor's mapping).

## May land early

None.
