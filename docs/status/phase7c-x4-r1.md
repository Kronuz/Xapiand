> Status: CURRENT (2026-11-02) the X4 security re-check's conditions (X4-R1 to R4) are closed and proved on `phase7c/x4-r1`; not pushed, awaiting the reviewer's probes.

# phase7c X4-R1 (build): close the re-check's conditions

Branch `phase7c/x4-r1` in `Xapiand-wt/x4-r1`, from master `Xapiand:10ab134ad`. Source: `docs/reviews/phase7c-x4-security.md`, "Re-check 1"; mechanism `docs/mechanisms/lua-bounds.md`; design D10, T10.

## State

Done and proved; local only. R1: no uninterruptible stretch of a script call passes `Limits::max_call_work` (16 x 10^6 units). R3: the shipped tests now kill the mutants the re-check found. R4/R2: stated in the changelog, the scripting guide, `lua-bounds.md` and D10.

## Done criteria

- [x] **R1.** `note()` condemns a call (reason `call`, 400) once the work since the hook last ran passes `max_call_work`: a linear call is refused before it runs, a call of many small charges (a `gsub`) when their sum passes. The matcher's step budget is further capped at `lua_xcallwork() / 5` steps (3.2 x 10^6; a fifth, not a quarter, so the step error comes before the sum of the charges does). `string.rep` now charges 3 units a repetition (a one-byte 31 MiB repeat held a reactor about 0.12 s at the old eighth of a unit a byte). The limits patch and its pinned hash moved together; `test_lua_patch_drift` passes.
- [x] **R3.** The close-handler shapes raise `error({})`; `Processor::printed_bytes()` lets `test_print_is_bounded_per_call` assert the logged bytes; `Processor::foreign_trusted()` lets `test_a_cached_foreign_script_is_trusted_for_a_second_and_then_read_again` cover the 1 s revalidation.
- [x] **R4, R2.** Changelog and `scripting.md`: `LUAI_MAXSTACK` 200,000 (`table.unpack` over about 200,000 elements errors), a script over a document of more than 262,144 value units answers 413 even if it touches one field, the new per-call cap. `lua-bounds.md`: the conversion's 0.7 s bound, the instruction-cap overrun by re-arming (at most 25%). D10 and T10 state the 20 ms bound.

## Measurements (Release, this host at load 5 to 7 on 16 CPUs, other agents running: noisy; the lead measures on `main`)

Probes in `.scratch/review-x4-recheck/` (`r1_flood.py`, `r1_hold.py`, kept in the main checkout's `.scratch/review-x4-r1/`); before = the binary of `Xapiand:10ab134ad`, after = this branch.

Flood: 16 clients, each on one persistent connection re-sending the request as soon as the 400 returns, 12 s, default node, fresh-connection light `GET /`:

| Script | before p50 / p99 | after p50 / p99 |
|---|---|---|
| 8 MiB `find('.-b')` (the re-check's) | 697 / 2,125 ms (n=14) | 5.1 / 13.1 ms (n=283; the 8 MiB repeat is refused at once) |
| 1 MiB `find('.-b')` (quadratic) | 380 / 1,176 ms (n=21) | 35 / 93 ms (n=168) |
| control: yielding Lua loop | 2.0 / 70.5 ms (n=307) | 2.0 / 62.2 ms (n=318) |

Hold of one reactor by one call (one HTTP server; a sampler `GET /` every 0.5 ms; worst GET overlapping the PUT; median of 5, `r1_hold.py`):

| One call | before | after |
|---|---|---|
| 1 MiB `find('.-b')`, `a*a*a*b`, `.-.-.-b`, `[ab]*c` | 341, 341, 343, 257 ms | 22, 19, 22, 22 ms |
| 1 MiB `find('(.-)%1c')` | 109 ms | 10 ms |
| 2 MiB `find('.-b')` | 642 ms | 26 ms |
| quadratic plain `find`, 512 KiB in 256 KiB | 211 ms | 11 ms |
| 8 MiB `find('.-b')` | 679 ms | refused at the `rep`, 1 ms |
| `rep('x', 31 MiB)` | 120 ms | refused, 1 ms |
| `gsub('a','b')` over 1 MiB (completes) | 31 ms | 31 ms (below the cap) |
| `upper`, `utf8.len` over 2 MiB (complete) | 15, 14 ms | 14, 13 ms |

The worst single call at the cap holds the reactor 19 to 26 ms; a completing call below it, the 1 MiB `gsub`, 31 ms (baseline GET during the control PUT, with its indexing: 6 to 20 ms). Per unit that is 1.2 to 1.9 ns on this host.

## Proof

- Unit tests: Release 31 of 31, ASan+UBSan (`detect_leaks=0`) 31 of 31, each rebuilt at the head. TSan on the Lua tests (`test_lua`, `test_lua_patch_drift`): 2 of 2, 0 reports.
- Docs E2E (newman 6.2.3, `harness/engine_e2e.sh`, Release): 641 of 641 (371 requests).
- `harness/lua_bounds.py` on the Release node: PASS. 33 bound cases (three new: 1 MiB quadratic pattern, `upper` over 8 MiB, `gsub` over 8 MiB, all 400), 10 swallowers 400 with 20 of 20 GETs, foreign scripts, light GET p50/p99/max 0.54/0.98/1.02 ms alone, 0.52/1.03/2.02 beside 4 long scripts, 0.50/1.40/1.48 beside 4 library-call loops, **0.53/57.95/88.45 ms beside 16 flooding clients** (the new live case; limit `--max-flood-p99-ms` 100; all 1,436 flood requests answered 400). The print-flood case builds its 8 MiB string with `rep('pppppppp', 2^20)` now.
- `fuzz_lua` under ASan+UBSan: seeds 1 and 2, 30 s each, no violation (39 and 88 scripts). Seed 7 fails at 60 s with "call 0 differs: sequential too large, interleaved error" on a script that builds a cycle (`_doc.cyc = _doc`) and a 160 KB shared blow-up in one document: **pre-existing**, the identical failure on the X4 binary (`Xapiand-wt/x4` ASan build, `Xapiand:2752325bb` code). The conversion's first failure (depth error on the cycle, or the byte budget) depends on the table's traversal order, which Lua seeds per state, so sequential and interleaved runs can disagree; the oracle should treat the two as one class. Not touched here.
- Reverted fix, test fails (mutants on `src/lua/processor.cc`, Release `test_lua`, `.scratch/mut2.py`, results `.scratch/mutants-r1.txt`):

| Mutant | Result |
|---|---|
| R1 reverted whole (no stretch cap, no matcher cap) | KILLED: `test_one_call_cannot_hold_the_loop_past_the_call_cap` (upper, reverse, utf8.len, rep, and more) |
| R1 stretch cap dropped alone | KILLED, same test |
| R1 matcher cap dropped alone | KILLED: the 1 MiB quadratic pattern's "step budget" and the tight-limit case |
| R1 cap doubled | KILLED |
| R3 both layers dropped (condemned flag and per-instruction raise) | KILLED (the call never ends, the 150 s timeout; it survived with string errors) |
| R3 flag alone, per-instruction raise alone | SURVIVE, as the re-check accepted (each layer suffices; defence in depth) |
| N1 print budget never spent | KILLED: `test_print_is_bounded_per_call` (it survived the first draft of the new test: a large print zeroes the budget, so the small prints now run first) |
| N1 print line cap dropped | KILLED |
| S4 revalidation default 1 h | KILLED |
| S4 `compile` always trusts the cache | KILLED |

## Done

- `Xapiand:dace91053` the per-call cap, `lua_xcallwork`, the R3 tests, R4/R2 docs, harness flood case.
- `Xapiand:7dc39f5fd` `rep` charged by repetition; the print test spends the budget on small prints first.
- (this commit) the harness print-flood case, this ledger.

## Next

The reviewer's probes (`ra_stall.py`, `rc_flood.py`) against this head. Follow-ups: the fuzz oracle's cycle versus byte-budget class; byte-copy calls still weigh an eighth of a unit a byte, so a copy of the full 64 MiB memory cap (`..`, `format('%s')`) can take about 50 ms; the conversion (0.7 s) is accepted and stated.

## Blocked on

Nothing.
