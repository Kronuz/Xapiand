# sortable-followups: journal

State: ready for review
Quest: .campaign/xapiand/quests/sortable-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code and docs committed in the sortable worktree (sortable:2e85e15). Nothing; all jobs passed.

## Next

1. The lead pushes the branch for the asan CI check (A4).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/mechanisms/sortable.md`: "with no undefined behavior: every shift count is bounded" |
| A2 | 261109-172719-1f0b22 (done at sortable:2e85e15): cmake -DSORTABLE_FUZZ=ON with Homebrew clang 23 (fuzzer,address,undefined), 722 s, 73,198,582 runs, exit 0, no crash or timeout, from make_corpus.py seeds plus fuzz/corpus. 261109-172720-e7310a (done): D5.9 2^1024 mutant, time to the kill, 4 seeds each: old fuzzer and corpus median 10.0 s (4.2-21.4); new fuzzer, old corpus 1.1 s (0.9-1.5); old fuzzer, new corpus 3.7 s (2.0-4.4); new fuzzer and corpus 1.1 s (0.8-1.3). Every kill was `accepted, not canonical: 01fc3f`. Reviewer's single sample at 937c7c6 was 60.5 s. |
| A3 | `docs/mechanisms/sortable.md`: "The readers are lenient in three ways, all accepted"; probe 261109-172802-a74c02 confirmed all three |
| A4 | build-test 261109-172754-1f56b7 done, exit 0 at sortable:2e85e15; asan is CI after the lead pushes |

## Result

Changed in Kronuz/sortable (2e85e15), no encoded byte or CLI reply changed: `fuzz/decode_fuzz.cc` runs the canonicality and conversion oracle on the whole input and on both halves; `fuzz/corpus` adds 190 raw encodings; `docs/mechanisms/sortable.md` states that every `number` converts without undefined behavior, lists the CLI's lenient forms, and describes the new fuzz checks and seeds. Effect: D5.9's 2^1024 mutant dies in a median 1.1 s (4 seeds) against 10.0 s before, and 3.7 s from the new seeds alone; a 722 s ASan+UBSan run (73.2M runs) is clean. Leaves open: Linux and arm64 fuzzing (not run).

## Rulings I made

1. The mechanism note did not contain the "can overflow" sentence (it was only in sortable-proof's journal Rulings 5), so A1 adds the correct statement rather than rewording one.
2. `tools/make_corpus.py` is outside the file set. The unprefixed seeds are committed raw encodings in `fuzz/corpus` (190 files named by their hex: every 1009th vector, the first and last two of each kind and length, the extremes of the order); the fuzzer runs from make_corpus.py's directory plus `fuzz/corpus`. The ".scratch" probe `fuzz_run.py` (not committed) drives the jobs.
3. The oracle on halves also runs the conversion checks (`accept()` is the same function for the whole and each half).

Lead, 11-09: diff read (docs note, fuzz halves via accept(), 190 raw seeds); low risk, no review needed. CI dispatched: sortable run 42737478382.
