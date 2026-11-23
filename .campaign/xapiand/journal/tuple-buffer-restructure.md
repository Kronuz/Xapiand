# tuple-buffer-restructure: journal

State: waiting on jobs 261122-222818-8766ed 261122-222822-3c4220 261122-222825-02a99a
Quest: .campaign/xapiand/quests/tuple-buffer-restructure.md (the lead's; never edit it)

Code: prospect worktree /Users/kronuz/code/Kronuz/prospect-wt/xapiand/tuple-buffer-restructure,
branch quest/xapiand/tuple-buffer-restructure, pushed. The move at prospect:01f1fd0, the layout
note at prospect:4430396 (head).

## Doing

The move is committed and pushed; its evidence jobs are queued.

## Next

1. Read the three jobs. members: expect 23 moved, 0 errors, 0 warnings (a local run at
   prospect:01f1fd0 printed exactly that). fnsizes: explain each changed row here (A2); if run
   building (`drain`, `group_run`, `sort_*`) or the codec (`encode_entry`, `decode*`, `view`)
   inline differently, add an exclusive A/B of prospect's ingest benchmark.
2. A1: read GitHub CI on prospect at 44303965ec66 (every platform); the `build-test` check reads
   the macos-arm64 job.
3. After split-check-members lands: rerun the `members` check (`campaign verify
   tuple-buffer-restructure`), whose path is cpp-libraries main's split_check.py.

## Tools

cpp-libraries' quest branch split-check-members at cpp-libraries:2df69e8, from its worktree
/Users/kronuz/code/Kronuz/cpp-libraries-wt/xapiand/split-check-members: `tools/outline.py`,
`tools/split_check.py --members`, `tools/fnsizes.sh`, `tools/selfcontained.sh`. Job scripts in
`~/.scratch/xapiand/tuple-buffer-restructure/` (fnsizes_diff.sh after index-restructure's,
selfcontained.sh); fnsizes tables land in its `fnsizes/`.

## The map

Applied with `outline.py apply include/prospect/tuple_buffer.hh MAP --inline` on main's
prospect:4f1ffa3, then `inline` dropped from the two member templates (R3):

```
class: tuple_buffer
umbrella-include: "prospect/tuple_buffer.hh"
piece-include: "prospect/{}"
includes-comment: // tuple_buffer's stages after ingest, one header each.
keep-one-line: yes
tuple_buffer_run.hh: tuple_buffer's run building: a batch's buckets grouped by term, sorted, and drained into a run, with its scratch memory
    drain group_run composite split bytes_of sort_groups radix_sort sort_run key_at charge_scratch
tuple_buffer_codec.hh: tuple_buffer's run entry codec: a term's postings encoded as an entry, decoded, and relabeled for a flush's numbering
    view encode_entry decode_entry decode relabel
tuple_buffer_read.hh: tuple_buffer's read side: the k-way merger over the runs' terms, a flush's terms with their postings, and the scan of every posting
    merger::merger merger::next merger::prefix merger::before merger::sift each_term postings_of scan
```

| Header | Lines | Members (main's lines) |
|---|---|---|
| `tuple_buffer.hh` | 751 (was 1,229); class body 540 (was 1,024) | ingest: `add`, `reserve_entry`, `set_for`, `push`, `push_to`, `spread`, `room`, `charge_columns`; flush planning: `flush`, `drain_for_flush`, `plan`, `add_documents`, `field_plan`, `splitters`; one-line accessors |
| `tuple_buffer_run.hh` | 246 | `bytes_of` 700, `charge_scratch` 780, `drain` 794, `group_run` 1014, `composite` 1083, `split` 1093, `key_at` 1115, `sort_groups` 1124, `radix_sort` 1148, `sort_run` 1171 |
| `tuple_buffer_codec.hh` | 141 | `view` 572, `encode_entry` 879, `decode_entry` 945, `decode` 961, `relabel` 990 |
| `tuple_buffer_read.hh` | 174 | `merger::merger` 598, `merger::next` 631, `merger::prefix` 666, `merger::before` 676, `merger::sift` 681, `each_term` 451, `postings_of` 978, `scan` 464 |

23 member definitions moved (432 body lines). Layout note: prospect docs/source-layout.md.

## Blocked on

Nothing. The `members` check as configured runs cpp-libraries main's split_check.py, which has no
`--members` until split-check-members lands; job 261122-222818-8766ed runs the quest branch's tool
meanwhile.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | CI on prospect at 44303965ec66 after the push (pending) |
| A2 | members 261122-222818-8766ed (queued; cpp-libraries:2df69e8); fnsizes 261122-222822-3c4220 (queued) |
| A3 | selfcontained 261122-222825-02a99a (queued) |
| A4 | prospect docs/source-layout.md at prospect:4430396 |

## Result

<Pending the jobs.>

## Rulings I made

- R1 The merger stays a nested class; its five multi-line members move to tuple_buffer_read.hh,
  and its one-line `entries()` stays. The lead's note says the merger is defined outside the body
  already: at main it is defined inside the class (main's lines 591-696). Moving the nested class's
  definition out of the body would leave it incomplete where `field_plan` and `flush`, which stay
  in the class, construct one (non-template `field_plan` would not compile), and `split_check
  --members` proves member moves, not a nested class leaving. So its code moves and its
  declarations stay.
- R2 Ingest and flush planning stay defined in the class, as the quest's Outcome says ("stay in
  the class"); `bytes_of` (used only by `group_run` and `split`) and `charge_scratch` (the drain's
  scratch, the quest's "batch memory") go with run building.
- R3 No `inline` on the two member templates (`each_term`, `scan`): a template needs none, and
  clang gives `inlinehint` only to functions declared `inline` (an in-class definition is
  implicitly inline, without the hint). The 21 non-template definitions need `inline` for the ODR
  in a header, so they gain the hint; the fnsizes diff shows what that changes.
- R4 Nested types with members of their own (`growing_bytes::room`, `tuple`'s accessors,
  `chunk_key::operator<`, `live_posting::operator<`, `no_extra`) stay whole, as uinteger_t's
  small nested types did.
- R5 The pieces carry no license block, like the 11 prospect headers whose first line says what
  they hold; each piece's first line is its comment (SPLITTING.md).
- R6 fnsizes compares a plain Release build (prospect's own and the A2 criterion's), not
  Release+LTO: prospect is header-only, so every tuple_buffer function compiles in its test's one
  translation unit and LTO adds no cross-unit inlining of it.
- R7 docs/ARCHITECTURE.md's table names `prospect/tuple_buffer.hh` only; outside the file set, so
  left as is (the umbrella still holds the buffer).
