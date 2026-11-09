# sortable-proof: journal

State: waiting on jobs 261109-143159-afc24c 261109-143200-4055ce
Quest: .campaign/xapiand/quests/sortable-proof.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs at sortable:f6dc3d4 still running: A2 fuzz hour 261109-143159-afc24c (ends about 15:36), A4
preflight 261109-143200-4055ce (mac hardened, mac ASan+UBSan and Linux ok; FreeBSD running).

## Next

1. When A2 and A4's preflight finish: their rows, the fuzz run count in Result, State "ready for review".
2. Lead: push quest/xapiand/sortable-proof and run CI on it (ci.yml triggers on push to main/master,
   pull_request or workflow_dispatch only: `gh workflow run ci.yml --ref quest/xapiand/sortable-proof`
   or a PR) for A4's CI part and the `asan` (ci:linux-asan-ubsan) check.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261109-143158-fe1ab9: `checked 4294967296 floats`, `all floats passed`, 6 min 05 s (Release, light) |
| A3 | 261109-143200-a3b59d (build-test): 5 of 5 tests, `sortable_cli` 40 s |

## Result

At sortable:f6dc3d4 (so far):

1. `tools/cli.cc` (`sortable_cli encode|decode`) and `tools/check_cli.py` (CTest `sortable_cli`): 213,491
   encode lines, 741,467 decode lines and 382,806 decode-then-encode lines, 0 mismatches against the
   model. Probes: a CLI mutant printing integers as `f` gave 113,403 failures; one accepting short `f`
   hex failed the malformed-line check.
2. `test/exhaustive_float.cc` covers and counts all 2^32 patterns: 4,294,967,296, 0 failures, 6 min 05 s.
3. `fuzz/decode_fuzz.cc` checks every conversion of an accepted value (Rulings 4).
4. Docs: `docs/mechanisms/sortable.md` ("How the decoder is proven", "The command-line driver"),
   `docs/status/sortable.md` items 8-10. Ledger item 9 states the fuzz hour's outcome ahead of
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
6. README and AGENTS.md (not in the file set) do not list `tools/cli.cc`; the mechanism note and the
   ledger do. The lead may add the repo-map line.
