# uinteger-restructure: journal

State: waiting on jobs 261123-032414-71300f 261123-032417-a8012a 261123-032421-5ecc9a 261123-032451-e05f05 261123-005923-69c83b 261123-005923-f76b83 261123-005924-9b4e8d 261123-005924-542f59
Quest: .campaign/xapiand/quests/uinteger-restructure.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1 committed at uinteger_t:db44669 (move uinteger_t:303d2a1, ab.sh uinteger_t:393c114, docs
uinteger_t:db44669), pushed. Code: uinteger_t worktree
`/Users/kronuz/code/Kronuz/uinteger_t-wt/xapiand/uinteger-restructure`. The mover is my own,
`.scratch/uinteger-restructure/mover.py` there (written before the lead's msg pointing at
index-restructure's move_members.py; kept, not switched mid-move): `mover.py ORIG OUT [--report]`
regenerates `uinteger_t.hh` and `uinteger_t/*.hh` from master's header (`orig.hh`); its MAP table is
the family map. Job scripts in `~/.scratch/xapiand/uinteger-restructure/` (fnsizes_diff.sh after
index-restructure's).

Jobs: build-test 261121-222414-b91586 (A1); members 261121-222415-33a3b7 (A2, expected to fail
until split-check-members lands); fnsizes diff 261121-222518-fe23a8 (A3); selfcontained
261121-222522-c21125 (A5); A/A mul 261121-222535-06b7c7, A/B mul 261121-222540-d43319, A/A conv
261121-222548-f7f06b, A/B conv 261121-222601-a73e84 (A4; exclusive is capped at 10 minutes, so
split by op: add mul sqr, then div str10 parse10 str16; sizes 2..4096; raw output in
`~/.scratch/xapiand/uinteger-restructure/{aa,ab}-{mul,conv}`). Cancelled, mis-submitted (zsh kept
`$Q` as one word): 261121-222431-04e4fd, 261121-222441-e5c94f, 261121-222451-5906a0,
261121-222457-f2b02e.

Probe 261121-221610-a2e851 (unversioned): every header twice-included alone and test.cc,
property.cc, public_api.cc compile with -Wall -Wextra, no diagnostics. A line multiset check: the
only lines of master's header found nowhere in the new tree are 354, one signature line per moved
function.

Mutation tooling, uinteger_t:5dd518f (the lead widened the files to `tools/**`): `mutate.py`
copies `uinteger_t/` and applies each edit to its own header; a set's `header:` is the default and a
`header:` inside a mutant names the header of the edits after it. Defaults: divq, kernels
`div_schoolbook.hh`; newton `div_recursive.hh`; ntt `ntt_transform.hh`; repr `arithmetic.hh`; toom,
toom4 `mul_toom.hh`. 66 edits of moved bodies lose one tab; forced_redo, forced_redo_noreload and
mul64-hi were anchored on the next member's in-class signature and now anchor on the moved code
(forced_redo_noreload's second edit is in `arithmetic.hh`). Migration scripts in
`~/.scratch/xapiand/uinteger-restructure/mutants/` (locate2.py, rewrite.py; equiv.py is the proof).

Mutation jobs: `--check` on every set at head 261121-223626-d524b3, at master 261121-223630-0744ab;
equivalence 261121-223633-b5b630 (equiv.py: each of the 355 mutants removes and adds the same lines,
indentation stripped, at head as at master, same tests, same equivalent marks). Full runs, 15 chunks
of at most 30 mutants (chunks.txt there; jobs.txt maps each id to its set), head: 261121-223655-7060a5 261121-223707-66d5fb 261121-223718-b4402d 261121-223730-c9a014 261121-223738-12ce4d 261121-223745-fa1a75 261121-223750-f985fb 261121-223757-b31995 261121-223802-646643 261121-223807-d3361c 261121-223812-5b0b0e 261121-223817-e54127 261121-223822-fd48f6 261121-223832-74a9ef 261121-223840-7687a2; master:
261121-223700-cd1d87 261121-223712-9f4b7c 261121-223724-2cad83 261121-223734-c8f23e 261121-223741-5ba34c 261121-223748-005c95 261121-223754-b3101c 261121-223800-3ee005 261121-223805-f83f5f 261121-223810-a80b9e 261121-223814-836fb4 261121-223819-0e7029 261121-223827-dac4d6 261121-223835-1d2df7 261121-223844-f7858b. Compare per set: killed counts at head must equal master's (and MUTATION.md's table: divq
32, toom 34, toom4 76, ntt 119, newton 43, kernels 21, repr 13); a drop is a finding to fix.

The earlier evidence jobs ran at uinteger_t:db44669; the head is uinteger_t:5dd518f, which changes
only `tools/`, `docs/MUTATION.md` and `AGENTS.md`.

## Update 2026-11-23 (head uinteger_t:5fedce2)

Lead's note: split-check-members landed (cpp-libraries:bc16496); `--members` refuses a declaration that
keeps a noinline attribute. Fixed at uinteger_t:2523f7a: the 5 container members (`reallocate`,
`grow`, `replace`, `copy_to_heap`, `insert`) lose `UINTEGER_T_NOINLINE` on the in-class declaration;
the moved definitions (`inline UINTEGER_T_NOINLINE ...`) keep it. A hand edit of the 5 declarations
(`mover.py` is scratch), the same shape outline.py writes. Proofs, all at 5fedce2: members check
261123-005919-29dc48 (it failed at 5dd518f: 261123-003509-c59b83); `g++-16 -Werror -Wall -Wextra`
Release build of every target (`~/.scratch/xapiand/uinteger-restructure/gcc_werror.sh`) at the head
261123-005925-703604 and, failing-without, at 5dd518f 261123-005720-a21e01.

Reading the mutation jobs (Next 1) found a real defect in my migration, uinteger_t:5fedce2:
kernels `threshold_mid_p_not_trimmed` was killed at head (22 killed) and equivalent at master (21).
Its regex kept master's indentation and header `mul_toom.hh`, so it matched the Toom lambdas, not the
two Karatsuba product trims of master; it now edits `mul.hh` with the dedented regex. The equiv job
(261123-005812 first run died on a `Counter.update` bug in equiv.py, fixed in scratch) is the test:
at 2523f7a it must fail (261123-005936-d61208, fails-without), at 5fedce2 pass (261123-005925-a0ec5a);
the kernels head set rerun 261123-005926-8319ef must kill 21. All other sets: head = master (divq
32, newton 43, ntt 119, repr 13, toom 34, toom4 76), 355 mutants apply at both, 0 build errors
(summary: `~/.scratch/xapiand/uinteger-restructure/mutants/summarize_jobs.py`). Those jobs ran at
5dd518f; only `uinteger_t.hh`'s five declarations changed since, not a mutated header.

Evidence jobs rerun at 5fedce2: build-test 261123-005918-1ec261; A5 261123-005922-940970; A3
261123-005922-f78320; A4 A/A+A/B mul 261123-005923-69c83b / 261123-005923-f76b83, conv
261123-005924-9b4e8d / 261123-005924-542f59. Old A4 outputs kept in
`~/.scratch/xapiand/uinteger-restructure/{aa,ab}-{mul,conv}.db44669`.

A3 at 5fedce2 (261123-005922-f78320 failed, as the old script fails on any difference): 95 of 97
binaries differ, 124 distinct text symbols (the lead's note said 3 of 97: that read only the log's
tail). Every difference is an inlining shift, none a body change (members: 354 moved, 0 errors).
Total `__text` per binary: -1.4% to +5.0%, median +1.1%; the 2 binaries that match are the
smallest. The job now compares `__text` by symbol and fails on a changed symbol its ALLOW table
(in `fnsizes_diff.sh`, one regex group per reason) does not name, or a binary whose total moved by
more than 6%; `__gcc_except_tab` rows are local labels numbered by emission order and are left out.
Reasons (symbols named in the script; sizes are the clang Release build on this Mac):

| Reason | Symbols | Example |
|---|---|---|
| R1 callee inlined into its callers, symbol gone | `k_absdiff`, `k_sqr_toom3`, `k_div_3by2`, `k_mod_1`, `divmod_small`, `bitwise_or`, `bitwise_lshift`, `_from_digit_bytes`, `_parse`, `_tree_encode`, `ntt_fwd3_layer`, `ntt_fwd_last`, `ntt_inv_first`, `k_div_newton_scratch_size`, `_emit_digits_n`, `_enc_fixed`, `_enc_fixed_n`, `operator<<=` | `k_absdiff` 448 B gone |
| R2 caller changes with what it inlined | `k_mul_karatsuba` 928 -> 1856 (absorbs `k_absdiff`), `k_sqr` 320 -> 688 (absorbs `k_sqr_toom3` 368: sum equal), `k_sqr_karatsuba` 640 -> 1056, `k_ntt_mul` 3280 -> 2976, `ntt_fwd_leaf` 416 -> 656, `ntt_inv_leaf` 416 -> 672, `single_divmod` 672 -> 816, `from_digits`/`_from_digits`, `_encode_radix`, `_enc_fixed_k`, Toom evals, `k_div_*` | `k_mul_karatsuba` |
| R3 emitted out of line now | `mult(const&, const&)` (the 3-argument `mult` is the one inlined), `ntt_forward`, `operator<<` for 8-bit types | `ntt_forward` 448 B |
| R4 test and bench helpers holding inlined library code | `main`, `check*`, `chunk_digits`, `heap_born`, `hex`, `make_count`, `make_dividend`, `ones`, `pow2`, `random_value` (and their `.cold` parts) | `make_count` 224 -> 640 (`uinteger_t_wide_shift`), 496 -> 768 (`_shift16`), 304 -> 1168 (`_shift32`): `bitwise_or`, `bitwise_lshift` inline into it |
| R5 std instantiations | `unique_ptr<radix>::~unique_ptr`, `vector<unsigned char>::resize`, `__throw_length_error`: Mach-O has no symbol sizes, so fnsizes reads the gap to the next symbol and these absorb neighbors | `resize` 6272 -> 6864 |

The cause is definition order: out-of-class definitions are emitted in family order, not class
order, which moves clang's bottom-up inlining decisions; no function body changed. Whether the
shift costs time is A4's question (jobs below). The job's table is the evidence, not the
allowlist: it only keeps a later head from adding an unexplained symbol.

## Next

1. Read A3 (261123-032414-71300f) and the GCC jobs (261123-032417-a8012a at the head,
   261123-032421-5ecc9a fails-without at 5dd518f); put their ids in Evidence.
2. Read A4 (261123-005923-69c83b / -f76b83 mul, -9b4e8d / -542f59 conv, queued exclusive): tables
   into `docs/perf/` with the A/A spread and hosts, honest magnitude per operation and limb count.
3. Step 2 (detail free functions) on its own commit, after the lead agrees the map (A7).

## Notes for tools/outline.py (msg from the lead)

What uinteger_t needed from a mover: nested-class members (`uinteger_t::container::`, dedent two
tabs); return-type qualification of member types (`digit`, `container::iterator`); default template
arguments dropped from out-of-class template headers; default arguments dropped; ctor-initializers
cut from the declaration; `explicit`/`static` dropped; a function returning a function pointer
(`void (*_pick_split(unsigned, unsigned))(const radix&, digit, unsigned char*)`); class-level
`#define`/`#undef` pairs that bracket bodies must move with them; column-0 preprocessor inside
bodies is left unindented; one-line bodies (`{ return _p; }`) on the signature line.

## Family map

The class body keeps data members, nested types, cutoffs (with their `#ifdef` overrides) and one
declaration per member function, each with its comment. Every member function body moves, as an
`inline` out-of-class definition, to the header of its family. Defs counts member functions (plus
the free functions in `operators.hh`); Lines is the header's length.

| Header | Members | Defs | Lines |
|---|---|---|---|
| `uinteger_t.hh` | macros, traits, the class (1,607 lines, was 5,709), the includes | - | 1,799 |
| `container.hh` | `container`'s 48 members; value `reserve`..`back`, `trim`, `resize_uninit`, `value`, `operator[]`, `bits` | 76 | 410 |
| `word.hh` | `_bits`, `_mult`, `_mullo`, `_multadd`, `_divmod`, `_addcarry`, `_subborrow` | 7 | 344 |
| `kernels.hh` | `k_add_n`..`k_divexact_1` (add, sub, mul_1, addmul/submul, shifts, basecase mul/sqr, absdiff, neg, divexact) | 18 | 289 |
| `mul.hh` | `k_mul_uses_*`, `k_sqr_uses_*`, scratch sizes, `k_sqr_karatsuba`, `k_sqr`, `k_mul_karatsuba`, `k_mul` | 11 | 221 |
| `mul_toom.hh` | `k_toom3_eval`..`k_mul_toom4` (Toom-3, Toom-4) | 9 | 258 |
| `ntt_transform.hh` | `ntt_prime`, `ntt_root`, `_mul64`, `_mulhi64`, `_div64`, mod arithmetic, layers, `ntt_forward`, `ntt_inverse` | 25 | 316 |
| `ntt_mul.hh` | `ntt_words`..`k_ntt_mul` (plan, load, pointwise, put, CRT) | 11 | 207 |
| `div_schoolbook.hh` | `k_inv_1`, `k_inv_3by2`, `k_div_3by2`, `k_div_sb_digit`, `k_div_sb`, `k_div_sb_trunc`, `k_cmp_n` | 7 | 191 |
| `div_recursive.hh` | `k_div_dc*` (Burnikel-Ziegler, 90 lines, joins Newton) and `k_div_newton*` | 14 | 232 |
| `bitwise.hh` | `bitwise_and/or/xor/inv`, `lshift`, `rshift`, `shift_count`, `bitwise_lshift/rshift` | 21 | 387 |
| `arithmetic.hh` | `long_add`, `add`, `long_sub`, `sub`, `long_mult`, `mult`, `set_digit`, `single_divmod`, `k_div_uses_trunc`, `knuth_divmod`, `divide`, `divmod`, `divmod_small`, `k_mod_1`, `mul_add_small` | 24 | 438 |
| `radix_tables.hh` | `chr`, `ord`, `base_bits`, `base_size`, `base_chunk`, `_limbs_for_digits`, `_digits_for_limbs`, `_umulh`..`_table_for` | 17 | 329 |
| `radix_encode.hh` | `_emit_digits`..`_encode_radix`, with the `UINTEGER_T_UNROLL` define/undef around `_enc_fixed` | 12 | 302 |
| `radix_decode.hh` | `_load_be`, `_store_be`, `_store_bytes`, `_digits_append`, `_digits_radix`, `_swar8`, `_chunk_value`, `_horner_limbs`, `_from_digit_horner` | 10 | 170 |
| `radix_tree.hh` | `_tree_*`, `_digits_tree`, `_from_digit_tree`, `_from_digit_bytes`, `_from_digits` | 14 | 331 |
| `digits.hh` | `digits`, `from_digits`, `str`, `strtouint`, `_parse`, `bin`, `oct`, `hex`, `raw` | 9 | 193 |
| `operators.hh` | `uint_0`, `uint_1`, `_uint_t`, constructors, conversions, member operators, `compare`, `mul`, public `divmod`; the free operators, `std::to_string`, `operator<<(ostream&)` | 69 + 23 free | 476 |

354 member functions in all (306 of `uinteger_t`, 48 of `container`). The small nested types
(`k_scratch`, `small_divisor`, `radix*`, `ntt_*`) keep their few inline members: they are
nested-type definitions, which stay whole.

## Step 2 candidates (not this turn)

Static members that read no object state (they take `digit*` spans and use only class constants),
which could become free functions in a detail namespace, the mpn layer:

- `word.hh`: all 7 (`_bits`, `_mult`, `_mullo`, `_multadd`, `_divmod`, `_addcarry`, `_subborrow`).
- `kernels.hh`: all 18 `k_*`.
- `mul.hh`, `mul_toom.hh`: `k_mul`, `k_sqr`, `k_mul_karatsuba`, `k_sqr_karatsuba`, the Toom kernels,
  the `k_*_uses_*` predicates and scratch sizes (they read the cutoffs).
- `ntt_transform.hh`, `ntt_mul.hh`: every `ntt_*`, `_mul64`, `_mulhi64`, `_div64`, `_ntt_plan`,
  `k_ntt_mul`.
- `div_schoolbook.hh`, `div_recursive.hh`: every `k_inv_*`, `k_div_*`, `k_cmp_n`.
- `radix_*.hh`: `_umulh`, `_pow2_div`, `_bit_length`, `_make_div`, `_div_magic`, `_emit_digits*`,
  `_split_chunk*`, `_enc_*`, `_swar8`, `_chunk_value`, `_horner_limbs`, `_load_be`, `_store_be`.

Not candidates: the value-level members (`add`, `divide`, `_from_digits`, ...), which take
`uinteger_t&`, and the radix tables, which publish into static state.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261123-005918-1ec261 at 5fedce2; GCC 16 at CI's configuration (RelWithDebInfo `-O2 -g -D_GLIBCXX_ASSERTIONS`, plus `-Werror -Wall -Wextra`, build and ctest) 261123-032417-a8012a; fails without it at 5dd518f (the `-Wattributes` errors): 261123-032421-5ecc9a, and under the old Release configuration 261123-005720-a21e01. CI on the pushed branch |
| A2 | members 261123-005919-29dc48 at 5fedce2 (354 moved, 0 errors, 0 warnings, 1 note); fails without: 261123-032451-e05f05 at 5dd518f (earlier: 261123-003509-c59b83). The equivalence job 261123-005936-d61208 at 2523f7a was meant to fail without the migration fix and passed: it compares each mutant's edit text, which was the same on both sides; the defect was where the edit applied (kernels killed 22 at head, 21 at master). Its proof is the kernels set at 5fedce2, 261123-005926-8319ef: 24 mutants, 21 killed, 3 equivalent, equal to master (261121-223724-2cad83, whose chunk is the first kernels one); equiv at 5fedce2 261123-005925-a0ec5a: 355 mutants, 0 differ |
| A3 | 261123-032414-71300f (replaces 261123-005922-f78320, which failed on every difference); reasons in the A3 table above |
| A4 | A/A 261123-005923-69c83b (mul), 261123-005924-9b4e8d (conv); A/B 261123-005923-f76b83 (mul), 261123-005924-542f59 (conv); queued exclusive, no result yet |
| A5 | 261123-005922-940970 |
| A6 | `docs/source-layout.md`, `README.md`, `ARCHITECTURE.md`, `CHANGELOG.md` at the head |
| A7 | the lead reads the family map above |

## Result

<When ready for review.>

## Rulings I made

- Comments stay with the declarations in the class: the class reads as the library's documented
  interface, the family headers hold the code. A family header's first line says what it holds.
- Signature changes on the definitions beyond split-check-members' list: a member type in a return
  type is qualified (`uinteger_t::digit`, `uinteger_t::container::iterator`), which an out-of-class
  definition needs; default template arguments are dropped like default arguments
  (`template <typename T, typename>`).
- Class-level preprocessor stays in the class (cutoff overrides, `#ifndef` defaults), except
  `UINTEGER_T_UNROLL`'s define and undef, which bracket `_enc_fixed` and move with it to
  `radix_encode.hh` (left in the class, the undef would precede the bodies that use it).
- The free operators, `std::to_string` and the stream operator move from the end of
  `uinteger_t.hh` to `operators.hh`, unchanged.
- Mutants name a header per edit, not one per set: forced_redo_noreload edits `div_schoolbook.hh`
  and `arithmetic.hh`, and every set spans several family headers; a set default plus a per-mutant
  `header:` states the exceptions only.
- `mutate.py` still passes `-G Ninja` only when `ninja` is on the PATH (it is, on every host here);
  making it unconditional is left for the lead, since the evidence jobs ran at uinteger_t:5dd518f.
- Each family header includes `../uinteger_t.hh` only: the umbrella keeps every standard include,
  so a family needs nothing else to compile alone.
- The `members` check's one note, `uinteger_t.hh` at 1,799 lines over `--max-lines 1000`: the file is
  the class (1,607 lines): data members, nested types, cutoffs and one documented declaration per
  member. Splitting it further would cut the class definition, which C++ keeps in one place;
  SPLITTING.md's size flags a look, not a split. No change.
- GCC at CI's configuration passes `-Werror -Wall -Wextra`; Release (NDEBUG) with `-Werror` stops
  at `test/test.cc:59` `unused variable 'base'` (used only in `assert`), identical on master, so
  pre-existing and outside this quest: a follow-up for the lead (`(void)base;` or `[[maybe_unused]]`
  in test.cc). The proof therefore uses CI's configuration, which keeps asserts live.
- A3 passes with an allowlist of 7 regex rows naming 86 symbols (the table under Doing) plus a 6% bound on
  each binary's total `__text`; the lead's expectation of 3 binaries was wrong (95 of 97 differ),
  and no whole-library regression shows in the totals (-1.4% to +5.0%).
