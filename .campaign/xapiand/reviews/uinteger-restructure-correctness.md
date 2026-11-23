# uinteger-restructure review: correctness

Commit: 5fedce24 (uinteger_t:5fedce240dc1, quest/xapiand/uinteger-restructure; base uinteger_t:8a6fe90)
Verdict: APPROVE
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Members check 261123-005919-29dc48: 354 definitions moved, 0 errors, 0 warnings. Per SPLITTING.md
  it proves the following. Each body is unchanged except for indentation. Each head is the
  declaration with only the listed edits. The class equals the base's with each definition replaced
  by its declaration, so default arguments, `constexpr`, `noexcept`, `explicit` and `static` all
  stay. Every non-template out-of-class definition is `inline` or `constexpr`. No member type is
  unqualified in a return type. This answers most of questions 1 and 2 mechanically.
- Build and test 261123-005918-1ec261.
- GCC 16 at CI's flags with `-Werror` 261123-032417-a8012a: 95 of 95 tests. This rules out a
  missing `typename` or `template`, and GCC's `-Wattributes` on the noinline move.
- Self-contained 261123-005922-940970: 18 of 18.
- Function-size diff 261123-032414-71300f: 0 changed symbols outside the allowlist, and every
  binary is within 6%. I read the whole log. I did not rerun it.

## What I checked myself

The scripts are under `.scratch/xapiand/review-uinteger-restructure-correctness/`. Each takes
commits and extracts them with `git archive`, so it can be rerun at a fix.

1. Macro environment, which the members check does not see (a read of the diff). Moving a body
   changes which class-level `#define`s it sees. Master's class body defines 11 macros (master
   `uinteger_t.hh` 2723-4945). Every conditional inside a body tests either a macro from the
   prelude or a user override. The one in-class macro a body tests is `UINTEGER_T_SPLIT_SHAPES`,
   which is defined before `_pick_split` both in master (4505 before 4510) and at the head (class
   1365, use `radix_encode.hh:88`). The `UINTEGER_T_UNROLL` define/undef pair moves with
   `_enc_fixed`. No body changes meaning.
2. Name lookup (a read of the diff).
   - All 354 return types are qualified, are template parameters, or are builtin or `std` types.
   - All 7 constexpr members keep `constexpr` on their definitions (`container.hh:137`,
     `ntt_transform.hh:7,11`, `radix_tables.hh:7,16,41,66`).
   - Keyword counts match master's, doubled only where a definition repeats its declaration:
     static 269/269, explicit 20/20, `= default` 5/5, noexcept 42 to 82.
   - The free operators and `std::to_string` sit after every member definition in `operators.hh`
     (`operators.hh:319`). Bodies therefore see the same namespace-scope names they saw at the
     class's closing brace.
3. ODR, job 261123-050111-c9ec34 (`odr/odr.sh`). Three translation units are linked into one
   binary at -O0 and -O2, under clang and g++-16, at master and at the head. One of them includes
   `uinteger_t/digits.hh` first. All 8 builds link and run, and they print identical output.
4. Noinline, job 261123-050207-ede6f8 (`noinline/noinline.sh`). Under clang and g++-16 at -O3,
   `reallocate`, `grow`, `replace`, `copy_to_heap` and `insert` remain out-of-line symbols at the
   head, exactly as at master.
5. Single-header contract, job 261123-050132-25e2bc (`contract/contract.sh`). I copied
   `uinteger_t.hh` and `uinteger_t/` onto an include path and built from another directory. Every
   case built and ran under clang and g++-16:
   - angle and quoted includes;
   - an earlier `-I` directory holding decoy `kernels.hh`, `operators.hh` and
     `uinteger_t/kernels.hh` with `#error` (not picked up);
   - the header reached through both a symlink and a copy;
   - `examples/demo.cc`.

   The single file alone no longer builds, as CHANGELOG says. No Kronuz repository vendors the
   single file; base-x and Xapiand FetchContent the whole tree.
6. Allowlist reasons, job 261123-050155-9de073 (`fnsizes/shifts.sh`). I rebuilt property (Release
   flags), wide_shift32 and bench at both sides, then compared sizes and call targets in the
   disassembly.
   - `k_mul_karatsuba` 928 to 1856: master calls `k_absdiff`, and the head no longer does. That
     symbol (448 B) is gone, which is consistent with two inlined call sites (R2 is true).
   - `k_sqr` 320 to 688: 320 + 368 for `k_sqr_toom3` is exactly 688. The head's `k_sqr` now calls
     `k_toom3_eval`, `k_toom3_interp` and itself, so `k_sqr_toom3` is inlined into it (R2 is true).
   - `make_count` (wide_shift32) 304 to 1168: its call to `bitwise_or` is gone and a call to
     `container::grow` appears, so `bitwise_or` is inlined into it (R4 is true).
   - `check_repr` (+2992) is the largest shift in property. It is a test helper covered by R4.
     `k_ntt_mul` now calls `ntt_forward` in place of `ntt_fwd` and `ntt_fwd3_layer`, which matches
     R1 and R3.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | nit | `radix_decode.hh` is the one header that is not a single family. Next to the Horner decoder (`_load_be`, `_swar8`, `_chunk_value`, `_horner_limbs`, `_from_digit_horner`) it holds the output side: `_store_be`, `_store_bytes` and `_digits_append`, plus `_digits_radix`, which calls `_encode_radix`. Its first line has to say "digit output" to cover them. A maintainer looking for the power-of-two and base-10 output path would open `radix_encode.hh` and not find it. | `uinteger_t/radix_decode.hh:1`, `:15-67`; callers `digits.hh:33,52`, `radix_tree.hh:195` | For the lead's A7 call: move the four output members to `radix_encode.hh`, or to `digits.hh`, and leave `radix_decode.hh` as the decoder (about 110 lines). Or keep it and name it in the layout note. |
| F2 | no | nit | Two docs still describe one header. README sells "**One header, no dependencies.**", which A6 says should now read as a header plus a directory. TESTING.md opens "How a change to `uinteger_t.hh` is verified". | `README.md:33`, `docs/TESTING.md:3` | Reword both. |
| F3 | no | nit | The CHANGELOG entry is dated 2026-11-21 and links `[303d2a1]`. Both hold only if the branch lands without being rewritten. | `CHANGELOG.md` (the new 2026-11-21 entry) | Check at landing. |
| F4 | no | gap | A5 is weak by construction. Each family header includes only `../uinteger_t.hh`, which includes every family, so "compiles alone" says nothing about the dependencies between families. The journal states this as a ruling. It is fine for this layout, but it does not stand in for a dependency check if Step 2 moves kernels into a detail namespace. | `uinteger_t/*.hh:5`; job 261123-005922-940970 | None now; Step 2's proof should not cite A5. |
| F5 | no | nit | The allowlist matches symbols by name, so a later body change in a listed symbol would pass A3. The journal says so. Bodies being identical is A2's proof, not A3's. | `~/.scratch/xapiand/uinteger-restructure/fnsizes_diff.sh` ALLOW | None. |

## Not run

- A4, the A/B timing, is still queued. I neither ran it nor waited for it. This review says
  nothing about speed.
- MSVC: not checked. That covers `__declspec(noinline)` on the definition only and MSVC's
  quoted-include search.
- The R5 rows rely on Mach-O size gaps, for example `~unique_ptr<radix>` at 5728 to 6688 B in
  bench. I did not check them symbol by symbol.
- Clang and GCC on Linux, and other CI platforms: these are CI's job.
- The mutation-set equivalence jobs: I did not reread them.
