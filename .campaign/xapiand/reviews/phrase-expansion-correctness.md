# phrase-expansion review: correctness

Commit: prospect:facbd3eec57b637cda5df8994f6171f1b57e0df0
Verdict: CHANGES
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261120-112928-913880 (build-test at facbd3e): phrase_expansion (2,080 phrases against brute
  force, consecutive, offsets, NEAR, both slot scorings), expansion_bounds (7,046 answered, 1,027
  refused), fuzz_phrase_replay and the suite. I did not rerun these.
- 261120-112930-ced038 (19 of 19 mutants killed at facbd3e).
- The oracle (`test/phrase_expansion_fixture.hh` `occurs`/`pick`) is written independently of
  `multi_phrase_scorer` (backtracking over sets against augmenting paths), so A1's matching claim
  stands on it.

## What I checked myself

Probe job 261120-121122-2b50fb (passed, 112 s): `.scratch/xapiand/review-phrase-expansion-correctness/run.sh`
builds `probe.cc` against the quest checkout with an allocation counter, over a fixture world of
16 segments, 4,000 documents and a 6,000-term tail.

- P1 budget against what the query allocates: phrase `[*, *]` (2,582 slot terms) is charged
  7,725,002 bytes at construction; its scorers allocate 16,316,512 bytes in the largest segment and
  79,613,616 bytes over the 16 segments, none of it charged (10.3x the charge).
- P2 slices: pause() true at every check leaves the prefix's charge unchanged (1.0x), a glob 1.03x,
  a fuzzy word `abcd~2` 2.2x at check_every 64 (automata review N1, inherited). The resumed walk
  continues from its key: the same 1,291 terms, no re-expansion.
- P3 repeated words: 1, 2, 4 copies of `*` charge 3.86 MB, 7.73 MB, 15.45 MB: linear, no
  multiplicative growth across positions; max_expansions counts each copy (phrase_expand.hh:99).
- P4 a word refused by the query's max_expansions of 5: `error` walks 764 entries, `first` 1,321,
  `most_frequent` 6,993 (the whole dictionary, the same as unbounded) and charges all 3.86 MB first.
- Code read: expansion runs once per word in the constructor (phrase_expand.hh:93-111); `scorer()`
  per segment only reads the term_table columns `expand` filled (expand.hh:188), so nothing is
  re-expanded per segment. NEAR's matching (operators.hh `near`/`assign`) is a bipartite
  assignment per window, polynomial in slots and window, not a product over expansions. The
  query-mode bound logic (phrase_expand.hh:155-172) is equivalent to "refuse iff live terms exceed
  what is left" for all three limit types, and the tie goes to the word's own limit.
- Lucene/Tantivy (Apache/MIT sources under ~/.scratch/parity-src): `MultiTermIntervalsSource`
  expands and bounds per leaf (Intervals.DEFAULT_MAX_EXPANSIONS 128, throws), counting dead terms;
  `SpanMultiTermQueryWrapper`'s scoring rewrite collects index-wide distinct terms and throws past
  maxClauseCount; Tantivy's `PhrasePrefixWeight` truncates to max_expansions (50) per segment.
  `Intervals.unordered` deduplicates identical sources into a repeating source (distinct
  occurrences) but lets different sources overlap.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | The byte budget is not the memory bound the note claims. With max_expansions 0 (the default) the note says "the byte budget is the memory bound until it does"; but `scorer()` builds a postings iterator and a positions reader per slot term per segment, plus the slot scorer's own cursor (phrase_expand.hh:120-143, operators.hh multi_phrase_scorer ctor), and none of it is offered to `charge`. Measured: 16.3 MB in one segment and 79.6 MB over 16 segments against a 7.7 MB charge (about 6 KB per term per segment), so a budget sized to admit the walk admits a query whose scorers allocate 2x to 10x more. | Probe 261120-121122-2b50fb P1; docs/mechanisms/phrase-expansion.md "Bounds", "Defaults" paragraph | Either (a) offer the scorer's cursors to the budget in `scorer()` (a charge per term per segment), with a test in expansion_bounds_test that a budget admitting the construction but below one segment's scorer bytes refuses; or (b) correct the note: the byte budget bounds the walk only, and max_expansions is the only bound on the scorers' memory (state the per-term, per-segment cost), so the Xapiand half sets one. (b) is the smaller change; (a) makes the claim true. |
| F2 | no | defect | A `most_frequent` word refused by the query's max_expansions walks and holds its whole expansion before refusing: the outcome is fixed at the first live term past what is left, for every limit type, but query mode keeps the word's type (phrase_expand.hh:160), so `most_frequent` walks 6,993 entries where `error` walks 764 and charges the full 3.86 MB. | Probe P4 | In query mode (room < own) expand as `error` at `max(room, 1)`: the result and the error are the same, and the walk stops early. |
| F3 | no | gap | A sliced phrase is charged its matcher again per resumed slice (automata N1, inherited): 2.2x for a fuzzy word at check_every 64, once per word and segment. Over-counting only. | Probe P2 | With automata's N1 fix; nothing here. |
| F4 | no | gap | Parity with Lucene and Tantivy is in matching, not in where the bound applies: Lucene's intervals bound each leaf's expansion (dead terms counted) and Tantivy truncates per segment; prospect bounds the snapshot's distinct live terms. Prospect's answer is independent of the segment layout, which is the better rule, but a pattern of 100 terms per segment over 3 segments passes Lucene's 128 and fails prospect's 128. | MultiTermIntervalsSource.java:58-75, phrase_prefix_weight.rs:83-90 | One line in the note (and in the Xapiand half's docs) that the bound counts the snapshot's distinct live terms, not a segment's. |
| F5 | no | nit | NEAR gives each slot a distinct position (multi_phrase_scorer, landed): `qu*` NEAR `q*` does not match a document with one "quick". Lucene's `Intervals.unordered` (ES `all_of`, ordered false) lets different sources overlap and matches it; identical words agree with Lucene's repeating source. Expansions make overlapping slots common. Keeping distinct positions is a reasonable choice; the owner's call. | operators.hh multi_phrase_scorer `near`; Intervals.java:430-439, UnorderedIntervalsSource.java:32-55 | Keep and say it in the note, or offer an overlapping NEAR; never blocking. |
| F6 | no | nit | Identical words (`a* a*`) are expanded and charged once each (linear, P3); they could share one expansion. | Probe P3 | Optional. |
| F7 | no | nit | The note says slicing reaches the phrase through walk_options, but `expand` resumes a paused walk at once (expand.hh:168), so a caller cannot yield between slices of a phrase's expansion; pause() only bounds check granularity. | expand.hh:125-131, 168 | Reword the note. |

## Not run

- No fuzzing beyond the replay already in the build-test job; the one CPU-hour fuzz and the
  ASan/UBSan builds are the quest's "When done" and CI's.
- Scoring parity with Lucene's MultiPhraseQuery (summed idf times phrase frequency) was not
  compared: the slot-sum scoring is the landed multi_phrase_scorer's, outside this quest.
- No timing; the probe's byte counts are from one Release build on macOS (malloc_size), one world.
- No Linux or FreeBSD run of the probe.
