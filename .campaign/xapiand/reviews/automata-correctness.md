# automata review: correctness

Commit: prospect:cbe3fe7118db863fd819339c71335561add80724 (with automata:a289a073d7df, termdict:e3f6f548465f)
Verdict: CHANGES
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261120-045742-f84874 (expand_test at prospect:cbe3fe7 with the two refs, exit 0): 1,188 queries
  over own, pooled and key fields, several segments with deletes; per segment `intersect` equals
  the reference predicates over the dictionary with each info equal to `find_term`'s, paused and
  resumed; `expand` under no limit, `error`, `first` and `most_frequent` against the model's live
  terms; 6,574 dead terms skipped; the corrupt-df case.
- 261120-045743-bf9ae6 (automata ctest, fuzz corpus replay, termdict_test, exit 0): the library's
  brute force (every {a, b} string to 10, Unicode and invalid bytes, every cut of 2- to 4-byte
  sequences, globs past one state word, k 0 to 3 and k 200, fixed prefixes, depths 0 to 4,096,
  every seek target checked against the skipped keys, matchers on 4 threads); termdict's seek
  visits checked against `find_run`'s.
- 261120-045743-1ae13f (expand_mutants.py, exit 0): 10 of 10 caught, each read in the log.
- 261120-045745-65164b (prospect suite, 54 of 54).
- GitHub CI on the quest branches (gh, read only), all green including linux-asan-ubsan and
  linux-tsan: automata run 44743399825 at a289a07, prospect run 44743404047 at cbe3fe7, termdict's
  run at e3f6f54. This is A2's CI half and T5's sanitizer row, including the threaded test.

## What I checked myself

Scripts are in `.scratch/xapiand/review-automata-correctness/` in the home checkout: `probe.sh`
builds `make_probe.py`'s variant of expand_test and `mutate_s1.py`'s mutant in the quest's slot,
then restores both files.

- Probe 261120-064344-25f4cc (exit 0; the first run, 261120-063719-ddfc02, had a check that could
  not tell S1 apart and is superseded). At the commit the variant passes with 0 failures over
  1,890 queries: terms of 2,048, 2,049, 3,002 and 5,000 bytes, a run of 600 four-byte characters,
  a 2,100-byte pooled tag and a 2,101-byte key, `?*?*`, patterns matching nothing (`zz`, `\xFF*`,
  prefix `zzzz`), prefix "" and `*` (everything), globs of 70 and 1,001 elements, Levenshtein over
  2,048- and 2,100-character targets at k 0 to 2, with and without transpositions, fixed prefixes
  0, 1 and 3, and an empty target at k 2. A cancellation requested at the fifth check stopped a
  397-term walk after 96 entries at check_every 16. Mutant S1 (expand.hh:147's per-term charge
  removed) passes the suite's expand_test and fails the variant: prefix "" then expands 439 terms
  and charges 0 bytes (243,151 with the charge).
- Code read: automata.hh in full (glob closure and `next`, the Levenshtein band and swap band
  against the OSA recurrence, the matcher's dead prefixes, `seen` and `fixed_`, seek targets as the
  larger of two valid lower bounds, the walk's pause and literal range); expand.hh in full (the
  `first` cutoff across segments, `error`, `most_frequent`, liveness, resume); fielddict's
  field_cursor and termdict's `seek(from, visit)` (every pass starts at a restart or after the
  cursor's entry, and stray visits before a restart are harmless because restarts are absolute);
  term_table's hashed columns and their rollback. The pooled prefix is prefix-free
  (`put_field`, fielddict.hh:45-53).
- Pins (Q4): prospect CMakeLists.txt:13 pins termdict e3f6f548465f and :23 automata a289a073d7df,
  the heads of both worktrees, both on `origin/quest/xapiand/automata`; termdict's is declared
  before strata's, so it wins over strata's pin, and d7f5954 (automata's test pin, strata main's)
  is its ancestor. automata a289a07 and prospect cbe3fe7 sit directly on their `origin/main`.
  termdict e3f6f54 does not: its base is 8a85c2e and `origin/main` is b866c4e (a CI-only commit).
- AUT-R1 to AUT-R14, against prospect's `docs/design/automata.md` (the evaluation that rules the
  port) and the code:

| Finding | State |
|---|---|
| AUT-R1 one interface | Addressed: D5's contract is automata.hh:29-31; fst's concept is deleted in evaluation step 5, outside this quest (D10) |
| AUT-R2 seeking | Addressed: `next`, `matcher::seek`, the walk seeks on every dead key; checked against every skipped key |
| AUT-R3 inverted reader, seekable cursor | Addressed: `field_cursor` over termdict's `seek(from, visit)`; no run replay copy |
| AUT-R4 keys | Addressed by evaluation D9: the key field walks `keys_plugin` by restarted scans; tested |
| AUT-R5 liveness, O3 questions | Liveness built and mutant-tested; the O3 questions go with T4 to the phase5 quest (evaluation step 6) |
| AUT-R6 load-bearing limits | Built (k clamps at 2^62, charges); the per-term charge is untested (A1), the rows are not in prospect's docs (N3) |
| AUT-R7 cancellation, slices | Addressed and tested |
| AUT-R8 T6 | Deferred: the quest's "After landing" |
| AUT-R9 T4 | Deferred: evaluation step 6 |
| AUT-R10 test gaps | 1 (`decode(std::string&&)` deleted, automata.hh:81), 2, 3, 5 addressed; 4: 4-byte pieces and the corrupt df are in, 2 KiB terms pass my probe, a corrupt block only at the raw-termdict fuzz level (N5) |
| AUT-R11 combiners, one store | Superseded by evaluation D7 (synonym_scorer exists; constant score when a comparison needs it); one store, hashed `term_table` |
| AUT-R12 where code lives | Addressed: Kronuz/automata; termdict does not depend on automata; O5 dropped by the owner (Q4) |
| AUT-R13 postings-tail profile | Not placed anywhere (N4) |
| AUT-R14 ledger, CI | Addressed: CI green on all three branches |

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| A1 | yes | gap | The only bound on an unlimited prefix expansion, and the per-term part of every expansion's bound (Q2), is untested: removing expand.hh:147's charge passes the suite. A prefix builds no matcher, so prefix "" then charges nothing for any number of terms; the suite's charge test refuses at the matcher's first charge and checks only `charged > 0` (expand_test.cc:425-440). | Probe 261120-064344-25f4cc: S1 passes expand_test; the variant fails with 0 bytes charged for 439 terms | expand_test: `prefix("")` and `*` expansions whose charges are at least the expanded terms' bytes, and a budget below that refuses; S1 as a row of `expand_mutants.py` |
| A2 | yes | gap | Landing order (Q4): libraries first is right, but termdict's `main` moved to b866c4e after the branch's base 8a85c2e. A rebase or squash landing orphans e3f6f54, prospect's pin, once the quest branch goes. | `git merge-base` in the termdict worktree; prospect CMakeLists.txt:13 | Land automata (fast-forward) and termdict with a merge commit that keeps e3f6f54, check `git merge-base --is-ancestor e3f6f54 origin/main` in termdict, then land prospect; or re-pin prospect after a rebase and rerun the suite |
| N1 | no | defect | Charges have no release, and every `intersect` call builds and charges a new matcher: once per segment and once per resumed slice (expand.hh:168, automata.hh:425-434, 527-535). A sliced walk over a large dictionary is charged its matcher state per slice, so a budget can refuse a query whose live memory is small. Over-counting, never under. `*` over 439 terms charged 461,422 bytes of matcher state to 10,290 bytes of terms. | Probe 261120-064344-25f4cc | Charge the matcher once per expansion, or give `charge_fn` a release; the owner's call on the charge contract |
| N2 | no | nit | Allocations past the charge: `term_table::put`'s second copy of each term and its column, `out`, `most_frequent`'s ranked vector, and decoded impacts. Real memory is a small multiple of what is charged. | expand.hh:170-188, searcher.hh `add` | Charge them, or state the factor in the mechanism note |
| N3 | no | gap | The evaluation's D3 moves the strata-D11 limit rows (edit distance, pattern length, expanded terms, walk time) into prospect's docs; `docs/mechanisms/automata.md` has no Limits section, unlike values.md and term-lists.md. | docs/mechanisms/automata.md | Add the four rows with their mechanisms |
| N4 | no | gap | AUT-R13's T9 postings-tail profile and T8's test-parity rows have no home: evaluation step 6 names T4, T6 and T7, and the quest's "After landing" names the fuzz hour and the speed rows. Formats stay open until release, so this is not urgent. | prospect docs/design/automata.md step 6; quest file | The lead places them in phase5 or drops them by name |
| N5 | no | gap | A corrupted dictionary under a prospect segment is not walked by any test or fuzz target: `fuzz_automata` corrupts a raw termdict, and `field_cursor`'s `termdict::corrupt` to `corrupt("terms checksum")` path (fielddict.hh field_cursor `seek`, `next`) is never reached. Section checksums on first read make this reachable only with crafted bytes. | fuzz/fuzz_automata.cc:66-82; expand_test.cc | A crafted block in expand_test, as `check_corrupt_df` crafts a df |
| N6 | no | nit | The suite's cancellation test only checks that a pre-requested token throws; it never asserts the walk stops within `check_every` entries (strata T2). My probe shows 96 entries at check_every 16 after the fifth check. | expand_test.cc:410-424 | Fold the probe's cancel case into expand_test |
| N7 | no | nit | The empty term is checked at the library level only ({a, b} strings from ""); expand_test's terms are never empty. | automata_test.cc:241; expand_test.cc:57-61 | A note if the index refuses empty terms; a case otherwise |

## Not run

- The fuzz CPU-hour and the speed and memory rows: after landing, by the owner's ruling.
- Oracle parity with Xapian (T4): no expansion mode exists yet; Xapian semantics were not judged.
- Xapiand's mapping onto `prospect::expand` and fst's deletion: other steps, outside this quest.
- No local sanitizer run: CI's linux-asan-ubsan and linux-tsan are green at the reviewed commits.
