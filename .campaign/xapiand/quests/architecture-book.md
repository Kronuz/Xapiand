# architecture-book: The Architecture and Design book

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status architecture-book`.

## Outcome

A complete Architecture and Design book explains every component of the stack (strata, prospect, the
Kronuz libraries under them, Xapiand on strata) and its key algorithms, techniques and mechanisms, written
from the how-it-works notes in each repository's `docs/mechanisms/`. Source: the owner, 2026-10-27: "I hope
we are keeping enough documents to later do a complete and comprehensive 'Architecture and Design' book of
this whole thing, explaining all components and key algorithms, techniques, and mechanisms." The earlier
lead recorded it in its memory (`architecture-book.md`) and drafted the outline, strata
`docs/book/OUTLINE.md` (DRAFT, 2026-10-27, notes linked 2026-10-30); no plan or quest carried it.

## Acceptance criteria

- A1 [artifact] Every section of the outline has its how-it-works note, in the docs of the repository it describes (the outline listed 80 notes as MISSING on 2026-10-30): `docs/book/OUTLINE.md` with no MISSING entry
- A2 [artifact] Parked and rejected designs are on main, labelled with their status, not left only on branches (the owner's rule of 2026-10-27): the outline's source map naming each one's file on a main branch
- A3 [artifact] The book's chapters, written from the notes: strata `docs/book/`
- A4 [decision] When the book is written: a row in PLAN.md's Decisions (the owner said "later")

## Contracts

None: documentation only. Each landed mechanism's note stays its own quest's criterion (the skill's
`templates/mechanism.md`); this quest only closes the outline's gap list and writes the chapters.

## Decisions

- Blocked on the owner: "later" (2026-10-27). Until then every quest that lands a mechanism names its
  note in its journal's Result, which shrinks A1's list.

## May land early

Each note, and each parked design labelled and landed, as it is written.
