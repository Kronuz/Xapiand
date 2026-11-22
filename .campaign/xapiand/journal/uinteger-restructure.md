# uinteger-restructure: journal

State: waiting on jobs 261121-222414-b91586 261121-222518-fe23a8 261121-222522-c21125 261121-222535-06b7c7 261121-222540-d43319 261121-222548-f7f06b 261121-222601-a73e84
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

## Next

1. Read the jobs: A3 diff (explain each size change), A4 tables into `docs/perf/` with the A/A
   spread and hosts, A1/A5 results into Evidence.
2. members check (A2) once split-check-members lands; answer its warnings.
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

- Lead: mutation testing (`tools/mutate.py`, `tools/mutants/*.txt`) is outside this quest's file
  set and breaks with the move: each mutants file names one `header:` (`uinteger_t.hh`), its
  regexes match the old body indentation, and `Tree` copies only top-level headers, so its copy
  lacks `uinteger_t/`. Not in CI or the criteria. Options: (a) add `tools/**` to this quest's files
  and let me make `header:` per mutant, copy `uinteger_t/` in `Tree`, and rebase the regexes on the
  new indentation, proved by `mutate.py --check` on every set; (b) a follow-up quest. Recommend
  (a): otherwise the tool is broken from this landing on.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-222414-b91586 (build-test) pending; CI on the pushed branch |
| A2 | pending: 261121-222415-33a3b7 runs before split_check has a member mode (split-check-members) |
| A3 | 261121-222518-fe23a8 pending |
| A4 | A/A 261121-222535-06b7c7, 261121-222548-f7f06b; A/B 261121-222540-d43319, 261121-222601-a73e84; pending |
| A5 | 261121-222522-c21125 pending |
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
- Each family header includes `../uinteger_t.hh` only: the umbrella keeps every standard include,
  so a family needs nothing else to compile alone.
