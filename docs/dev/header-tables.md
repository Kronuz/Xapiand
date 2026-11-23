# Header static tables

A namespace-scope `static const` (or plain `const`) object with a dynamic initializer, defined in a
header, has internal linkage: every translation unit (TU) that includes the header gets its own copy
and its own initializer. This note lists Xapiand's tables of that kind, where each lives, and what
it costs in a Release+LTO macOS x86_64 build of `xapiand`.

## The tables

| Table | Where | Objects | TUs that include it (upper bound) |
|---|---|---|---|
| Content types | `src/database/data.h` | 12 `ct_type_t` (two `std::string`s each) and `msgpack_serializers` | 26 |
| Cmp values | `src/multivalue/keymaker.h` | 7 `std::string`; `SERIALIZED_*` call `Serialize::floating` | 4 |
| Soundex accents, composed, prefixes | soundex library headers, through `src/phonetic.h` | 7 `std::unordered_map` / `std::vector` | 4 |
| HTM start vertices | htm library `htm.h` | `start_vertices[6]` (`Cartesian`), `start_trixels[8]` | 35 |

`src/phonetic.h` only includes the soundex headers; the soundex and htm tables belong to their
libraries.

## inline const on Mach-O

The head (b853eefa0) marks data.h's and keymaker.h's objects `inline const`, which gives one object
per program. On ELF, clang keys each inline variable's initializer to the variable's COMDAT, so the
linker also keeps one initializer. Mach-O has no COMDAT: every including TU keeps its own guarded
`__cxx_global_var_init` function, and each runs at startup and checks the guard. The construction
itself runs once.

Measured at base 6cffdc7af against head b853eefa0 (size tables from jobs 261121-202620-10935d and
261121-202624-066a03, diffed by 261121-202633-71dbfb; `size -m` section totals from the same logs):

| Row | Base | Head | Delta |
|---|---|---|---|
| `_GLOBAL__sub_I_*` (per-TU init) | 103104 B, 53 functions | 87104 B, 49 functions | -16000 B |
| `__cxx_global_var_init*` | 4544 B, 100 functions | 52352 B, 424 functions | +47808 B |
| Initializers run at startup (`__init_offsets`) | 153 | 473 | +320 |
| `__gcc_except_tab` | 287968 B | 291764 B | +3796 B |
| `__bss` (the per-TU copies) | 2203652 B | 2189700 B | -13952 B |
| `__DATA_CONST,__const` | 81720 B | 80536 B | -1184 B |

The 324 new initializer functions are 286 of 144 bytes (data.h's 11 non-empty `ct_type_t`s times
26 TUs), 26 of 208 bytes (`msgpack_serializers`, once per TU), and 12 of 96 or 112 bytes,
consistent with keymaker.h's strings. Every data.h-only per-TU initializer (data.cc, mime_types.cc, wal.cc,
wide_search.cc) disappears, and each other including TU's sub_I shrinks by about 600 bytes.

Net, on macOS: static-initialization code grows 31808 bytes (+47808 - 16000) and its exception
tables 3796 bytes; the data copies shrink 13952 bytes of zero-fill memory, which costs no file
bytes. Startup is unchanged within noise (job 261121-202637-5516f1: `xapiand --version`, 30 runs,
head median 21.29 ms, base 22.04 ms, base A/A 22.19 ms, sd 0.8-1.8 ms, with 7 light holds running).

### What is not the change

- `__text` grows 52496 bytes in all, of which 17792 bytes are LZ4 and XXH64 code: the head binary
  links LZ4 statically and the base links `liblz4.1.dylib` (`otool -L` in both logs). The shared job
  slot's dependency builds were reconfigured between the two builds; the head commit touches only
  the two headers. The remaining 4448 bytes of `__text` are LTO inlining drift.
- About 6450 per-name changes are renumbered `GCC_except_table*` entries and other renames; the
  section totals above are the comparison.
- fnsizes attributes 7616 bytes of `__const` growth to `typeinfo name for main::$_2`, the gap
  after the last symbol; the `size -m` totals show no such growth.

A clean A/B builds base and head back to back in one job, from one dependency tree.

## One initializer on every platform

An `extern const` declaration in the header with one definition in `src/database/data.cc` and
`src/multivalue/keymaker.cc` gives one object and one initializer on Mach-O and ELF alike. No
namespace-scope initializer in another TU reads these objects (their other uses are function bodies
and default arguments), so moving their initialization into one TU adds no ordering hazard.

## Library rows

- **soundex**: `german_soundex.h`, `french_soundex.h` and `spanish_soundex.h` define 7 `static const`
  maps and vectors. The four including TUs (handler.cc, keymaker.cc, query_dsl.cc, serialize.cc)
  each carry a sub_I of about 12.5 KB at the head (12448-12912 B), about 50 KB in all, plus four
  heap copies of every table. One definition per table (a `.cc`, or constant tables) removes three
  of the four.
- **htm**: `start_vertices` builds 6 `Cartesian`s through a non-constexpr constructor in each of
  the 35 including TUs. The 17 TUs whose only remaining initializer is 192 bytes (bucket.cc,
  circle.cc, htm.cc, polygon.cc and others) are consistent with that table: about 6.7 KB of
  initializers if all 35 pay it, inferred from the per-TU sizes rather than proven. A constexpr `Cartesian`
  constructor makes the table constant-initialized and removes them.
