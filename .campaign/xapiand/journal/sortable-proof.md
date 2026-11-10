# sortable-proof: journal

State: ready for review
Quest: .campaign/xapiand/quests/sortable-proof.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: every criterion has evidence at sortable:937c7c6; the security review is next (the lead's).

## Next

1. Fold in the security reviewer's findings by id, each with a test that fails without it.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261109-145953-7b39eb: `checked 4294967296 floats`, `all floats passed`, 4 min 25 s (Release, light) |
| A2 | 261109-145954-894c13: `sortable_fuzz -max_total_time=3600 -timeout=5`, ASan+UBSan, 363,143,404 runs in 3601 s, 100,845 exec/s, 0 crashes, 0 aborts (an accepted input that fails to re-encode aborts) |
| A3 | 261109-145956-b3bf94 (build-test): 5 of 5 tests including `sortable_cli`; ci:linux-x86_64-gcc, ci:linux-x86_64-clang, ci:linux-arm64-gcc, ci:macos-arm64, ci:macos-x86_64, ci:freebsd-x86_64 (run 42717337992) |
| A4 | ci:linux-x86_64-gcc, ci:linux-x86_64-clang, ci:linux-arm64-gcc, ci:macos-arm64, ci:macos-x86_64, ci:freebsd-x86_64, ci:linux-asan-ubsan, ci:linux-tsan (run 42717337992); preflight 261109-161721-204fdf: mac-hardened, mac-asan-ubsan, linux, freebsd ok |

History at sortable:f6dc3d4 (same code but the README and AGENTS.md lines): 261109-143158-fe1ab9
exhaustive, `checked 4294967296 floats`, `all floats passed`, 6 min 05 s; 261109-143200-a3b59d
build-test, 5 of 5 tests; 261109-143200-4055ce preflight, mac-hardened, mac-asan-ubsan, linux and
freebsd ok; 261109-143159-afc24c fuzz hour, clean at 159M runs, about 116k exec/s, still running
when the head moved.

At sortable:937c7c6: 261109-145955-7f6914 preflight failed on a cpp-libraries preflight.sh bug
(shared source cache across job checkouts; the lead fixed it at cpp-libraries:eb0a58e, rerun
261109-161721-204fdf passed). 261109-143159-afc24c (old head) was cancelled at 1771 s, 226,185,320
runs, 0 findings: about half a further CPU-hour, history only.

Messages: the lead's mid-task message (push, README/AGENTS.md in the file set, then ready for
review): pushed run noted; README.md and AGENTS.md list `tools/cli.cc` (sortable:937c7c6); jobs rerun
at that head; ready for review follows their pass. The lead's second message (all jobs done, CI 42717337992
green): Evidence filled, State ready for review; ledger item 9 already says the hour was clean
(0 findings), so no docs change and no further commit in sortable.

## Result

At sortable:937c7c6 (two commits on 67b2a90; `sortable.hh` and its bytes unchanged, so the contract
holds):

1. `tools/cli.cc` (`sortable_cli encode|decode`, stdin lines) for T3's harness, and
   `tools/check_cli.py` (CTest `sortable_cli`) driving it against the model: 213,491 encode lines,
   741,467 decode lines (every vector, the reject witnesses, every 0-2 byte string, every 3-byte
   string starting with an escape tag, 200,000 seeded mutations) and 382,806 decode-then-encode lines,
   0 mismatches, 25-40 s. Probes: a CLI mutant printing integers as `f` gave 113,403 failures; one
   accepting short `f` hex failed the malformed-line check.
2. T2 exhaustive: all 2^32 float patterns counted and checked, 0 failures, 4 min 25 s.
3. T4: one CPU-hour, 363M runs, 0 findings under ASan+UBSan, with the target's new conversion checks
   (Rulings 4). With the earlier 40 CPU-minutes (279M runs) and the cancelled half hour (226M), the
   decoder has 868M fuzz runs without a finding.
4. Docs: `docs/mechanisms/sortable.md` ("How the decoder is proven", "The command-line driver"),
   `docs/status/sortable.md` items 8-10, README.md and AGENTS.md name the CLI.

Open: T3 itself (the Python port and `harness/sortable_xlang.py`) is step 3 of the design.

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
