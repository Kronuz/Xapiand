# sortable-followups: journal

State: waiting on jobs 261109-172719-1f0b22 261109-172720-e7310a 261109-172754-1f56b7 261109-172802-a74c02
Quest: .campaign/xapiand/quests/sortable-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code and docs committed in the sortable worktree (sortable:2e85e15). Waiting on A2's two jobs, A4's build-test and a CLI leniency probe.

## Next

1. Read the jobs; record the before/after D5.9 times and the 12-minute run.
2. Fill Evidence and Result; state "ready for review". The lead pushes the branch for the asan CI check.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | docs/mechanisms/sortable.md, "Every conversion is defined for every `number`, with no undefined behavior: every shift count is bounded and `-1022 - exponent` cannot overflow an `int`." |
| A2 | pending: 261109-172719-1f0b22 (12 min, ASan+UBSan, committed corpus), 261109-172720-e7310a (D5.9 mutant, before/after) |
| A3 | docs/mechanisms/sortable.md, "The readers are lenient in three ways, all accepted: an `i` number may have leading zeros (`i 007`) or be `-0` (the integer 0), and hex digits, in `f HEX16` and in `decode` lines, may be upper or lower case."; probe 261109-172802-a74c02 |
| A4 | pending: build-test 261109-172754-1f56b7; asan is CI after the lead pushes |

## Result

pending

## Rulings I made

1. The mechanism note did not contain the "can overflow" sentence (it was only in sortable-proof's journal Rulings 5), so A1 adds the correct statement rather than rewording one.
2. `tools/make_corpus.py` is outside the file set. The unprefixed seeds are committed raw encodings in `fuzz/corpus` (190 files named by their hex: every 1009th vector, the first and last two of each kind and length, the extremes of the order); the fuzzer runs from make_corpus.py's directory plus `fuzz/corpus`. The ".scratch" probe `fuzz_run.py` (not committed) drives the jobs.
3. The oracle on halves also runs the conversion checks (`accept()` is the same function for the whole and each half).
