# sortable-proof: journal

State: waiting on jobs 261109-143158-fe1ab9 261109-143159-afc24c 261109-143200-4055ce 261109-143200-a3b59d
Quest: .campaign/xapiand/quests/sortable-proof.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs at sortable:f6dc3d4: A1 exhaustive 261109-143158-fe1ab9, A2 fuzz hour 261109-143159-afc24c,
A4 preflight 261109-143200-4055ce, build-test (A3) 261109-143200-a3b59d.

## Next

1. When the jobs finish: fill Evidence and Result; ask the lead to push the branch and dispatch CI for A4's CI part.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|

## Result

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
