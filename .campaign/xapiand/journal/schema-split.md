# schema-split: journal

State: waiting on jobs 261122-212030-8090b3 (A2) 261122-212033-455700 (A1); CI run 45116992526 at Xapiand:235f61732
Quest: .campaign/xapiand/quests/schema-split.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Master merged (Xapiand:e9804943f, clean; touches no schema file). Allowlist row renamed
(Xapiand:235f61732, the lead's ruling) and pushed; CI 45116992526. A1 and A2 queued at that head
(the e9804943f pair, 261122-211513-d04385 and 261122-211517-3263ba, cancelled as superseded).

## Next

1. Read 261122-212030-8090b3 (A2: expect 0 errors, the 43 warnings answered below),
   261122-212033-455700 (A1) and CI 45116992526; record.

## Blocked on

Nothing. Lead's rulings:
1. Glob (2026-11-21): `src/database/schema/*.c*` and `*.h*` in CMakeLists.txt. Done:
   Xapiand:8d85af070.
2. split_check (2026-11-21): the anchor pass is cpp-libraries:fe8491f; the gate check reruns with
   it. The old checker gave 40 errors (261121-204542-68e0fc), all "moved lines starts/ends inside
   a function" on repeated `inline void` lines; the anchored checker gave 0 (261121-204551-d364fa).
3. Allowlist (2026-11-22): `harness/async_allowlist.tsv` joins the file set. CI at
   Xapiand:1d471a960 (run 44986514175) failed async_check --strict: row 22 allowed `schema.cc
   [pool] lock_shard` x2, and both hits moved with Schema::index to `schema/index.cc` (:155,
   :219). Done: Xapiand:235f61732 renames the row's path. Rows 72-73 still match schema.cc.

## Layout at the head

| File | Lines | Base lines |
|---|---|---|
| `schema.cc` | 505 | 1-75, 94-146, 2541-2617, 9270-9563 |
| `schema/reserved.h` | 1,006 | 76-93, 210-303, 355-394, 408-429, 446-476, 495-517, 563-701, 1300-1588, 1652-1913 |
| `schema/specification.cc` | 1,399 | 147-206, 305-316, 403-407, 438-445, 478-494, 519-562, 703-1299, 1623-1651, 1920-2471 |
| `schema/walk.h` | 419 | 2472-2540, 2618-2731, 4660-4693, 5883-5978, 6881-6912 |
| `schema/handlers.h` | 1,022 | 8278-9225 |
| `schema/dispatch.h` | 945 | 1915-1918, 5979-6833, 6868-6880 |
| `schema/index.cc` | 886 | 207-209, 2732-3543 |
| `schema/update.cc` | 1,039 | 3578-4544 |
| `schema/types.cc` | 748 | 4545-4548, 4607-4659, 4694-5313 |
| `schema/value.cc` | 731 | 3544-3577, 4549-4606, 5314-5882 |
| `schema/properties.cc` | 883 | 6834-6867, 6913-6975, 7609-8277, 9226-9269 |
| `schema/feed.cc` | 703 | 6976-7608 |
| `schema/fields.cc` | 756 | 318-354, 395-402, 430-437, 1589-1622, 9564-10159 |

The cut plan below was the first draft; this table (and cut.py) is the record.

## Map of schema.cc (10,159 lines at the base 57496b89a)

Line ranges at the base. "inline" means defined `inline` in schema.cc; split_check fails a dropped
`inline` (a removed `inline void` line reappears nowhere), so an inline function stays in the
translation unit of every caller or moves verbatim to a header.

| Lines | What | Binding |
|---|---|---|
| 1-127 | license, includes, L_SCHEMA, type-name `static_string`s, NAMESPACE_PREFIX_ID_FIELD_NAME, the index()/write_schema() outline | |
| 129-206 | unused `std::hash<const array>`; `def_accuracy_*` tables (dynamic init) | tables used by spec types and validation |
| 208-1905 | reserved-value vocabulary: accuracy/stop/stem/index/uuid-field/type/stem-language parsers and printers (`_get_type` 596 lines, `_get_str_type` 267, `_get_stem_language` 230, `_get_index` 137 are phf tables), `str_set_*` acceptable-value strings, `global_acc_prefix_*`, `default_spc`, get_accuracy_* (public, for query_dsl) | `static inline` helpers shared by every part below |
| 1908-1917 | repr_field (external, 28 uses in walks and field lookups), has_dispatch_* prototypes | |
| 1920-2470 | required_spc_t, index_spc_t, specification_t members | |
| 2472-2616 | Schema::check, constructor, get_initial_schema, get_*properties, clear | |
| 2618-2730 | restart_specification, restart_namespace_specification, FedSpecification, feed_subproperties | inline, called by all three walks' *_subproperties |
| 2732-3577 | Index banner: index, index_subproperties, index_new_object/object/array/fields/inner_object, index_item_value | index() and index_subproperties call the inline dispatch |
| 3578-4065 | Update banner: the update walk | update(), update_subproperties call the inline dispatch; update_fields/inner_object inline |
| 4066-4544 | Write banner: the write walk | write(), write_subproperties call the inline dispatch; write_fields inline |
| 4545-5312 | get_partial_paths, complete_specification, set_type_to_object/array (inline, all walks), validate_required_namespace_data, validate_required_data, fits_integer_type, guess_concrete_type | type detection and validation |
| 5314-5882 | index_partial_paths, index_term, index_item (inline, 429 lines: the value indexers), store_item, merge_geospatial_values | index_item only from index_item_value |
| 5883-5978 | update_prefixes, detect_dynamic | inline, all walks and add_field |
| 5979-6911 | dispatch entry points and phf tables: dispatch_process_*, dispatch_write_*, _dispatch_write/feed/process/process_concrete_properties, has_dispatch_*, dispatch_set_default_spc | all inline: the walks inline the tables |
| 6913-6975 | add_field (2) | |
| 6976-7608 | dispatch_feed_properties, feed_* (39) | reached only through _dispatch_feed_properties |
| 7609-7984 | write_* (schema writes, out of line; several call inline process_*) | |
| 7985-8276 | process_* for non-concrete fields, out of line (from _dispatch_process_properties) and process_meta/position | |
| 8278-8590 | process_* for concrete fields, inline (from _dispatch_process_concrete_properties) | inline |
| 8591-9195 | consistency_* (31), inline | inline |
| 9196-9224 | `#ifdef XAPIAND_LUA` write_script (inline), normalize_script | one top-level #ifdef: both stay in one file |
| 9226-9268 | set_default_spc_id, set_default_spc_version | |
| 9270-9415 | get_full, dispatch_readable, readable_* | |
| 9416-9562 | stop-word checks (anonymous namespace), get_modified_schema, get_const_schema, to_string | |
| 9564-10159 | _get_data_id, get_data_id/set_data_id/get_data_script, get_data_field, get_slot_field, get_dynamic_subproperties, get_prefixed_global | query-side field lookups |

## Cut plan (first draft, superseded by "Layout at the head")

| File | Holds | Base lines |
|---|---|---|
| `src/database/schema.cc` | the Schema object: construction, property access, readable form, stop-word checks, the walk outline | 1-127 (less the type names), 129-145, 2472-2616, 9270-9562 |
| `schema/reserved.h` | reserved-word vocabulary the pieces share, in `schema_detail`: type-name strings, value parsers and printers, extern acceptable-value lists and default accuracies, repr_field; has_dispatch_* (global, as now) | 76-92, 208-303, 356-394, 409-429, 447-476, 496-517, 564-700, 1301-1587, 1652-1917, 6377-6400, 6475-6550, 6881-6890 |
| `schema/specification.cc` | the field specification types and their tables (`_get_type`, default and global accuracies, acceptable-value lists, default_spc) | 147-206, 305-316, 403-407, 438-445, 478-495, 519-562, 703-1299, 1623-1650, 1920-2470 |
| `schema/walk.h` | inline Schema members the walks share: restart, feed cache and feed_subproperties, set_type_to_*, update_prefixes, detect_dynamic, dispatch_set_default_spc | 2618-2730, 4660-4692, 5883-5978, 6892-6911 |
| `schema/dispatch.h` | the reserved-property dispatch the walks inline: entry points, the phf tables, and the inline handlers (concrete-field process_*, consistency_*, write_script/normalize_script) | 5979-6202, 6401-6474, 6551-6833, 6868-6879, 8278-9224 |
| `schema/index.cc` | the document walk (Index banner) | 2732-3543 |
| `schema/update.cc` | the schema walks: update and write (Update and Write banners) | 3578-4544 less 4545 |
| `schema/types.cc` | type detection and validation of a field's first value | 4545-4659, 4694-5312 |
| `schema/value.cc` | indexing one value: terms, values and stored data | 3544-3576, 5314-5882 |
| `schema/properties.cc` | out-of-line reserved-property handlers: write_*, non-concrete process_*, _id/_version defaults, add_field | 6834-6866, 6913-6975, 7609-8276, 9226-9268 |
| `schema/feed.cc` | reading a stored schema's reserved properties into the specification | 6203-6374, 6976-7608 |
| `schema/fields.cc` | what queries ask of the schema: field and slot specifications by name, accuracy unit names | 318-354, 396-401, 431-436, 1590-1621, 9564-10159 |

Line ranges are approximate until cut.py's table is final; cut.py is the record.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261122-212033-455700 (suite at Xapiand:235f61732); 261121-204605-8fd08d passed 81/81 at Xapiand:8d85af070 |
| A2 | 261122-212030-8090b3 (gate split check at Xapiand:235f61732) |
| A3 | docs/dev/schema-layout.md |
| A4 | lead |

Ended jobs at Xapiand:8d85af070 (the glob head):
- 261121-204605-8fd08d suite: 81/81 passed
- 261121-204544-b0745e fnsizes, Release+LTO: classified under "fnsizes: base vs split"
- 261121-204547-0ba7f1 ctest -N: 81 tests, identical at the base and the split
- 261121-204554-38f339 reserved.h, walk.h, handlers.h, dispatch.h each compile alone
- 261121-204542-68e0fc split check, old checker: 40 errors (superseded by fe8491f)
- 261121-204551-d364fa split check, split-check-anchor's checker: 0 errors, 43 warnings

Scripts: ~/.scratch/xapiand/schema-split/{cut.py,fnsz_head.sh,fnsz.sh,selfcontained.sh,ctestn.sh}.

## fnsizes: base vs split (Release+LTO xapiand)

Base 261121-200713-a4eb7a (57496b89a, the merge base): 18,002 symbols, 10,151,037 B. Split
261121-204544-b0745e (8d85af070, only the split's commits on top): 18,114, 10,187,721 B. Delta
+112 symbols, +36,684 B (0.36%). Compared after normalizing names the split renames
(`schema_detail::`, `GCC_except_tableN`, LTO `(.N)` suffixes, `__FILE__`-length array
parameters, `_GLOBAL__sub_I_<file>`); 201 rows differ, every byte in one bucket
(fndiff.py, fncat.py in the scratch dir):

| Bucket | Symbols | Bytes | Why |
|---|---|---|---|
| Static initializers | +16 | +7,488 | each of the 8 new .cc files gets a ~800 B `_GLOBAL__sub_I_` for the header-level statics schema.h and its includes define (schema.cc's own dropped 6,672 -> 848; specification.cc's is 6,576, holding the moved tables); `__cxx_global_var_init` copies follow the same TUs |
| Header constants | +60 | +2,881 | `static constexpr` reserved-word strings (`RESERVED_TYPE`, `RESERVED_ACC_PREFIX`, `UUID_FIELD_NAME`, ...) from the headers: one copy per TU that reads one, as every header-internal constant |
| Exception constructors | +16 | +3,040 | `ClientError`/`MissingTypeError`/`Error` templates take `__FILE__` as `char const (&)[N]`; the new file names have new lengths, so a throw site's instantiation differs per file |
| Exception tables | +16 | +276 | follow the new functions above |
| LZ4 and xxHash | +1 | +17,992 | inlining moved by the split: LTO's inliner re-decided in lz4.c and xxhash (untouched sources, same pin): `LZ4_decompress_safe_{forceExtDict,withPrefix64k,withSmallPrefix}` now out of line, `LZ4_compress_fast_continue` 8,608 -> 12,768, `XXH64`/`XXH32` C copies replaced by two private `XXH64` copies; plus `dec64table` now a standalone symbol |
| Schema functions | +1 | +1,040 | inlining moved by the split: `get_slot` (utils.cc) now has an out-of-line copy (736 B) with callers in fields.cc, types.cc and specification.cc; `get_data_field` +1,040, `get_slot_field` -32, `consistency_accuracy` -288 |
| phf tables | 0 | -1,492 | `_get_*` lookup tables renamed into `schema_detail`; `_get_index_uuid_field`'s and `_get_str_type`'s tables shrink (-1,024, -464): the rest of each old symbol was neighboring data, see last row |
| Layout shifts | +2 | +5,459 | fnsizes sizes a symbol to the next address, so moved data and padding land on neighbors (the +2: lz4's `inc32table` split from its neighbor, one more `std::__tree` instantiation copy): `typeinfo name for main::$_2` +2,864, `prospect::term_table::column` +2,848 (the gap before it), and +/-16..48 B on ~100 unrelated std/strata symbols |

No schema function body changed size beyond the inlining rows named above, and no inlining on the
indexing path (`index_item`, the dispatch tables, the walks) changed: those stay in headers. The
LZ4/xxHash shift is the LTO cost model, not code: it changes no source and has no timing claim
either way; a compressor A/B would decide it if the lead wants one.

## Split-check warnings answered (43, with the anchored checker)

- 12 `using namespace schema_detail;` (schema.cc and every piece but reserved.h): the shared
  helpers moved into `schema_detail`; each TU looks them up as it did when they were file-scope.
  No name in `schema_detail` collides with a global the pieces use (each compiles; probe suite).
- 10 `extern` declarations in reserved.h and 10 dropped `static` on the same tables in
  specification.cc (`def_accuracy_*` x4, `str_set_*` x6): one definition, read by several pieces.
  Their dynamic initializers stay in specification.cc beside `global_acc_prefix_*` and
  `default_spc`, which read them, so the initialization order inside that TU is the base's.
- 8 dropped `static` in reserved.h (`static inline` helpers, now `inline` in `schema_detail`):
  one definition across TUs instead of one copy per TU; the fnsizes diff checks the code.
- 2 added `inline` in reserved.h (`_get_stemmer_name`, `repr_field`): a header definition needs
  it; `repr_field` was external, its only users are the schema pieces.
- 1 added `inline` on `normalize_script` (handlers.h): it shares the one top-level
  `#ifdef XAPIAND_LUA` with `write_script`, which is inline and must be in a header.

## Result

<When ready for review.>

## Rulings I made

- Headers hold the inline closure: the walks inline the dispatch tables and the tables inline the
  concrete-field handlers, so walk.h, dispatch.h and handlers.h hold them verbatim rather than
  dropping `inline` (which would change inlining on the indexing path).
- Schema::check lives in walk.h: a template the constructor and the update/write walks instantiate.
- Tables become one extern definition in specification.cc (not header `static const` copies):
  the quest says not to repeat them, and their initializers stay in one TU.
- The L_* debug toggles stay at the top of schema.cc: `L_SCHEMA` is defined there and used
  nowhere, and the `L_DEBUG`/`L_CALL` overrides are commented out, so every piece gets log.h's
  defaults, as the base did. Turning tracing on for a piece means copying the toggle into it.
- split_check.py: not copied to harness/; the fix belongs in cpp-libraries (Blocked on 2).

- schema.h stays whole: 552-1068 is one class body (can't be cut) and 1-550 the enums and
  specification structs it is built on; no caller needs one without the other.
