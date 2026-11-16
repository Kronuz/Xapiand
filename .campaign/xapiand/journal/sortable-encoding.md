# sortable-encoding: journal

State: waiting on jobs (head Xapiand:7700cb179, review fix P1): mutant 261116-042123-032811; build-test 261116-042126-a82cc5; e2e 261116-042126-6ee9e4; T6 261116-042128-885821; T5 261116-042129-1e9af0; C1 probe 261116-042137-a9c947; T8 build 261116-042217-f60d1c -> run 261116-042250-335a75; load build 261116-042224-c53ddf -> run 261116-042253-9e3c64; CI 43865811902 and FreeBSD 43865811733. Ready for review once they pass.
Quest: .campaign/xapiand/quests/sortable-encoding.md (the lead's; never edit it)

## Doing

Session 11-16 (fresh contributor), head Xapiand:beec843ec. Master merged (campaign files only).

1. T6's 6 wrong at c9d8bc9f8, two root causes, neither D6 nor strict_stod:
   - `t6d.d [-1, 45837296000001]` and `[-500000, 2177452800000001]` are datetime ranges (microseconds), not
     integers above 2^53. `GenerateTerms::datetime(double, double)` cast the ends to `time_t`, truncating toward
     zero: a start of -1 us or -0.5 s became 1970-01-01T00:00:00, so the decade/year cover named 1970 and dropped
     the 1969-12-31T23:59:59.5 and .999999 documents. Fix Xapiand:2c26c80e9 floors both ends (an indexed value's
     terms come from its floor). Reproduced on a local build (2 wrong), 0 wrong after. `test_generate_terms`'s
     datetime case gains -0.5 s and -1 us stamps below every boundary; fails-without job 261116-015759-0b2751.
     The pre-switch run 261115-232755-9ae906 did not show these two ranges; I did not find why (getDateQuery and
     generate_terms are unchanged by the switch); the truncation itself is pre-existing code.
   - Midpoint: strict_stod (strict-stox:d3eaf32) reads "9007199254740993.0000001" as 9007199254740994 (probe
     ~/.scratch/sortenc/stod.cc). The 400 is `index_item`'s floating case: a numeric field refuses a bare string
     value, on master too. The f ranges missed only document 1, the one whose PUT failed. The test now gives the
     text as `{"_float": "..."}` (Cast::floating, strict_stod): midpoint 0 wrong of 8 locally.
2. T5 "identity 1": harness, not a hang. The new build alone (light hold) answers all 1,064 searches in 228 s, the
   slowest 1.9 s (20,000-hit mtime sort); the job's 120 s socket timeout was host load. Xapiand:99ed2629d: 600 s,
   a search that still does not answer becomes a named wrong answer, each build prints its slowest search.
3. A12 Xapiand:fbc190ae9: case-preserving `serialis` -> `serializ` over src, tests, contrib, CMakeLists.txt (93
   files, 8 renamed), then AGENTS.md, ARCHITECTURE.md, README.md, value-encoding.md. Library calls kept:
   `MsgPack::unserialise`, `.serialise()` on MsgPack values, `cuuid::(un)serialise`, `UUID::(is_serialised|
   unserialise|serialise)`, and `using SerialisationError = SerializationError;` in exception_engine.h (cuuid's
   `.cc` files throw that name through `CUUID_EXCEPTION_HEADER`). The xapiand binary builds; tests build in
   build-test.
4. A8 Xapiand:b635c16a4: NOTICE, THIRD-PARTY-NOTICES, its generator, third-party-notices.md and copyright.md drop
   the GPL note; the generator reproduces THIRD-PARTY-NOTICES byte for byte against the pinned _deps.
   exact-id-keys.md drops the `old-codec` mutant. Ledger row Xapiand:beec843ec.
5. A6/A7: `tests/codec_load.py` (Xapiand:9414d5cd3, 4ce1f43c2): master e8f3cde1c vs head, 5 rounds ABBA, restore
   rate, size after a clean stop, sort and range searches/s at 8 clients x 15 s, A/A band from old's spread. Two
   same-binary dry runs: size varies about 7% (segment layout follows flush timing), restore about 6%.
   Local build kept at build-local/ (786 MB, ignored) for follow-up fixes; delete when done.

## Next

1. Review CHANGES (P1, blocking; N2 fixed, N1 left, D87 covers it). Fix: `numeric_bound` (src/multivalue/range.cc) reads a text bound
   as a value's text (int64, uint64, then double) whatever the field's type: Xapiand:7700cb179. Test first, Xapiand:59f3f86cf: T6's
   `text` group (odd integers above 2^53 on a floating field, string bounds in the DSL and in `q=f:a..b`, plus string bounds on
   `/t6w*/`). Fails-without job 261116-042123-032811 runs the test at 59f3f86cf (no fix). C1: probe job 261116-042137-a9c947
   (the reviewer's probe_text_bounds.py, copied byte for byte to ~/.scratch/sortenc, against the fixed build) for `--confirm P1=`.
2. Jobs superseded by the fix: build-test, e2e, T6, T5, A8, A12 reran at 7700cb179; the T8 and load chains were cancelled and
   resubmitted at 7700cb179 (slots sortable-t8, sortable-load). T3 (client port vs C++, untouched by the fix) stays at 34211bb50.
3. CI: 43865811902 (+FreeBSD 43865811733) at 7700cb179 replaces 43864388357.
4. When they finish: fill Result, confirm the ledger row's proof wording.

## For the lead

- A9 and the asan check need the branch pushed (I don't push).
- Outside my set and stale after the rename: `harness/coverage.sh:104` and `TESTING.md:22` name `src/serialise.cc`;
  `harness/async_check.py:63` and `harness/test_gate_tools.py:122` name `sortable_serialise` (gone). docs/** keeps
  about 40 pages with serialis* (A12 waits for the set to widen).
- `tests/exact_id_mutants.sh query-old-codec` likely no longer kills: ids and values share one codec, so
  `Serialize::id` and `Serialize::MsgPack` give equal bytes for an integer id. Not run.
- The T8 job writes a `src/serialize.h` alias header into the old tree at job time, so the bench source names only
  `Serialize::`.

## Blocked on

Nothing. (The 11-16 decision answered the A12 library question; the license files joined the set.)

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | D87 (PLAN.md decision row, owner 2026-11-13) |
| A2 | build-test 261116-042126-a82cc5 (pending) |
| A3 | T3 261116-041118-a44776 (passed at 34211bb50; the fix leaves the codec and client alone); mutants killed earlier: window-62 261115-224339-653d9d, rule-7 261115-224809-f01fd9 |
| A4 | T5 + E2E bodydiff 261116-042129-1e9af0 (pending) |
| A5 | T6 261116-042128-885821 (pending); fails-without: old build 261115-232755-9ae906, floor 261116-015759-0b2751 (passed), text bounds 261116-042123-032811 (pending); C1 probe 261116-042137-a9c947 (pending) |
| A6 | load 261116-042253-9e3c64 after build 261116-042224-c53ddf (pending) |
| A7 | T8 261116-042250-335a75 after build 261116-042217-f60d1c; load 261116-042253-9e3c64 (pending) |
| A8 | no-gpl 261116-042126-e622f7 (passed); license notes Xapiand:b635c16a4 |
| A9 | CI run 43865811902 at Xapiand:7700cb179 (pending; sortable pin 2fad5a2) |
| A10 | `docs/mechanisms/value-encoding.md` |
| A11 | `docs/src/content/docs/changelog.md` |
| A12 | grep 261116-042128-29afcc (passed) |

## Result

<When ready for review.>

## Rulings I made

- Pin sortable:915b014 (newest main): bytes and fixtures equal sortable:67b2a90 (sha256 of
  vectors.txt c4d4a425..., reject.txt a679d215..., model.py b8c29680... at 67b2a90, ac13b77, 915b014);
  it adds the trusted typed conversions D87's speed gate needs.
- D88 spelling: `Serialise` becomes `Serialize`, `Unserialise` `Unserialize`, `SerialisationError`
  `SerializationError`, `serialise*.{cc,h}` `serialize*` (the direct respelling).
- Python module path `xapiand/sortable.py` (not the GPL file's `xapiand/utils/sortable.py`, so the
  diff never shows the GPL text being replaced in place).
- An absent range bound encodes as -inf/+inf for every numeric type (design D6, review F4c).
- A range bound encodes in its own MsgPack type (int64, uint64 or double), not the field's: the codec is cross-type exact,
  so `x >= 6.5` on an integer field and `x >= 2^53 + 1` on a floating field compare exactly. A string bound parses as the
  field's type. Accuracy terms use the truncation of either, which is monotone, so the cover stays a superset.
- A floating field stores an integer value's exact bytes and terms (D1: one number, one encoding), not its double.
- NaN is refused at the user-value entry points; `Serialize::floating(double)` itself stays total (keymaker distances).
- rapidjson parses with `kParseFullPrecisionFlag` so a JSON decimal equals strict_stod's double for the same text (D6, F3).
- Pre-switch reference build for A4, A5 and A7's codec half: 8a7da1f4a (step 2's head, the old codec behind the one seam,
  same pins). A6 and A7's load half compare against master e8f3cde1c, as the criteria say.
- T5's file corpus: 20,000 files sampled (seed 20261116) from ~/code/Kronuz, sizes and mtimes only, frozen in the repo
  (259 KB) so the job and any rerun read the same bytes.
- T8 uses sortable's real-value set from sortable:722c30e (`docs/perf/data/real-values.tsv.gz`, 434,977 values, content
  sha256 7b5c20d8...), the frozen set its own speed note used; the bench source is appended to the old tree so both
  builds time one source with one set of flags.
- A numeric field keeps refusing a bare string value (user-visible behavior, unchanged); T6 sends decimal text through
  the `_float` cast instead.
- A12 renames the contrib cuuid ports (Python, JavaScript) with the rest of contrib: they are Xapiand's tree, not calls
  into Kronuz/cuuid.
- A datetime range's ends floor to whole seconds before its term cover is chosen, matching how a value's terms are built.
- A text range bound reads as a value's text does (int64, uint64, then double) whatever the field's type (lead ruling, review P1); a boolean bound is 0 or 1.
- N1 (the keymaker registry name) left: D87's dump-and-restore excludes a mixed-version cluster. N2 comment fixed in keymaker.h.
