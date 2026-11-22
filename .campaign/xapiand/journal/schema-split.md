# schema-split: journal

State: waiting on jobs 261121-204605-8fd08d 261121-204544-b0745e 261121-204547-0ba7f1 261121-204551-d364fa 261121-204554-38f339 261121-204542-68e0fc 261121-200713-a4eb7a
Quest: .campaign/xapiand/quests/schema-split.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Glob committed alone (8d85af070, the lead's ruling); A1, fnsizes, ctest -N, selfcontained and the
anchored split check queued at that head.

## Next

1. Diff fnsizes base (261121-200713-a4eb7a) vs head (261121-204544-b0745e); classify every row
   (moved, inlining change, new/duplicated initializer); restore lost inlining on the indexing
   path with `inline` in a detail header.
2. Read A1, ctest -N, selfcontained, the anchored check; record.
3. The gate `split` check (A2) reruns after split-check-anchor lands in cpp-libraries.

## Blocked on

Nothing. Lead's rulings (msg from the lead, 2026-11-21):
1. Glob: add `src/database/schema/*.c*` and `*.h*` after the src/database line; CMakeLists.txt is
   in the file set. Done: Xapiand:8d85af070. The quest file's "no CMake change is needed" was the
   lead's error, per the lead.
2. split_check: my patch is quest split-check-anchor (cpp-libraries:449d913). Its A3 is this
   split at 0 errors: job 261121-204551-d364fa (that worktree's checker, `--base master .
   src/database`, at Xapiand:8d85af070). The gate A2 `split` check (261121-204542-68e0fc, current
   checker, expected 40 errors) reruns after that quest lands.

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
| A1 | 261121-204605-8fd08d (suite at Xapiand:8d85af070) |
| A2 | 261121-204542-68e0fc (current checker, expected red); 261121-204551-d364fa (anchored checker, 0 errors expected) |
| A3 | docs/dev/schema-layout.md |
| A4 | lead |

Jobs at Xapiand:8d85af070 (the glob head):
- 261121-204605-8fd08d suite (A1)
- 261121-204544-b0745e fnsizes, Release+LTO (base: 261121-200713-a4eb7a at 57496b89a)
- 261121-204547-0ba7f1 ctest -N at the base and the split
- 261121-204554-38f339 the four new headers compile alone (selfcontained.sh)
- 261121-204542-68e0fc split check (gate); 261121-204551-d364fa split check, split-check-anchor's

Cancelled as superseded: the glob-patched probes 261121-203624-f53a56, 261121-203757-64078f, and
261121-203631-291e5b, 261121-203641-50bda7, 261121-203642-2768b9, 261121-203627-ff0c0a.

Scripts: ~/.scratch/xapiand/schema-split/{cut.py,fnsz_head.sh,fnsz.sh,selfcontained.sh,ctestn.sh}.

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
