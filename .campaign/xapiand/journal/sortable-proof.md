# sortable-proof: journal

State: waiting on jobs 261109-145953-7b39eb 261109-145954-894c13 261109-145955-7f6914 261109-145956-b3bf94
Quest: .campaign/xapiand/quests/sortable-proof.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head is sortable:937c7c6 (README and AGENTS.md list the CLI, at the lead's request; docs only, but
evidence is keyed by the whole tree, so every job reruns). Jobs at 937c7c6: A1 exhaustive
261109-145953-7b39eb, A2 fuzz hour 261109-145954-894c13, A4 preflight 261109-145955-7f6914,
build-test (A3) 261109-145956-b3bf94.

## Next

1. When they pass: the four rows, CI rows from the lead's run, State "ready for review".
2. Lead: push 937c7c6 and dispatch CI (the run 42716510581 is at f6dc3d4, no longer the head).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|

History at sortable:f6dc3d4 (same code but the README and AGENTS.md lines): 261109-143158-fe1ab9
exhaustive, `checked 4294967296 floats`, `all floats passed`, 6 min 05 s; 261109-143200-a3b59d
build-test, 5 of 5 tests; 261109-143200-4055ce preflight, mac-hardened, mac-asan-ubsan, linux and
freebsd ok; 261109-143159-afc24c fuzz hour, clean at 159M runs, about 116k exec/s, still running
when the head moved.

Messages: the lead's mid-task message (push, README/AGENTS.md in the file set, then ready for
review): pushed run noted; README.md and AGENTS.md list `tools/cli.cc` (sortable:937c7c6); jobs rerun
at that head; ready for review follows their pass.

## Result

At sortable:937c7c6 (numbers from the f6dc3d4 runs until the reruns pass):

1. `tools/cli.cc` (`sortable_cli encode|decode`) and `tools/check_cli.py` (CTest `sortable_cli`): 213,491
   encode lines, 741,467 decode lines and 382,806 decode-then-encode lines, 0 mismatches against the
   model. Probes: a CLI mutant printing integers as `f` gave 113,403 failures; one accepting short `f`
   hex failed the malformed-line check.
2. `test/exhaustive_float.cc` covers and counts all 2^32 patterns: 4,294,967,296, 0 failures, 6 min 05 s.
3. `fuzz/decode_fuzz.cc` checks every conversion of an accepted value (Rulings 4).
4. Docs: `docs/mechanisms/sortable.md` ("How the decoder is proven", "The command-line driver"),
   `docs/status/sortable.md` items 8-10; README and AGENTS.md name the CLI. Ledger item 9 states the fuzz hour's outcome ahead of
   261109-143159-afc24c; if that job fails, it is corrected with the fix.

## Rulings I made

1. CLI protocol (the design names only "`encode`/`decode` over stdin lines"): `sortable_cli encode`
   reads `i DECIMAL` or `f HEX16` (binary64 bits); `sortable_cli decode` reads hex (empty line = empty
   input) and answers `reject`, `i DECIMAL` (an integer in [-2^63, 2^64)) or `f HEX16` (NaN as
   7ff8000000000000). decode's answers are encode's input form, so a decoded line fed back re-encodes to
   its input. Malformed lines exit 2 rather than printing an answer, so a harness bug cannot pass as a
   mismatch-free run. This matches the Python port's D7 `decode -> int | float | None`.
2. A3's test drives the CLI against `tools/model.py` (a dry run of T3 with the model as the client):
   Python-dependent like `sortable_fixtures`, skipped without Python 3. No second, Python-free path.
3. `test/exhaustive_float.cc` now encodes every NaN pattern (was a stride of 257) and asserts its count
   is exactly 2^32, so A1's "every one of the 2^32" is checked by the program, not argued.
4. The fuzz target gained conversion checks (security lens: Xapiand calls them on users' bytes):
   saturate equals exact where exact exists, an integer's `to_double` equals `static_cast<double>`,
   saturation lies between 0 and the value, conversions keep the order of the split pair. Probe: a
   planted `saturate_int64` wrap died at once; a planted `to_double` truncation survived 60 s before the
   `static_cast` oracle and died at once after it.
5. Not fuzzed: `number` structs built by hand (an `exponent` far outside [-1074, 1024] can overflow
   `to_double`'s arithmetic); only `decode` builds them from disk bytes, and it bounds the exponent.
6. The README's exhaustive line ran `ctest` after `-DSORTABLE_EXHAUSTIVE=ON`, which never runs the
   exhaustive program (it is not a CTest); it now runs `build/sortable_exhaustive_float`.
