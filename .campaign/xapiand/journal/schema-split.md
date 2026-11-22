# schema-split: journal

State: working
Quest: .campaign/xapiand/quests/schema-split.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Cutting schema.cc by the plan below, one commit per new file, with a generator script
(~/.scratch/xapiand/schema-split/cut.py) that copies line ranges of the base file verbatim.

## Next

1. Cut, commit per file, push; split check, selfcontained and a probe build (CMake glob patched in
   the job only, not evidence) as jobs.
2. After the lead's ruling on the glob: suite (A1), fnsizes at the head, diff against the base job
   261121-200713-a4eb7a, classify every row.
3. docs/dev/schema-layout.md (A3).

## Blocked on

Lead: the source glob. CMakeLists.txt:1127 globs `src/database/*.c*`, not recursively, so a `.cc`
under `src/database/schema/` is not built (the quest says no CMake change is needed; it is). Options:
1. Add `"${PROJECT_SOURCE_DIR}/src/database/schema/*.c*"` to XAPIAND_SRC_LIST (one line,
   CMakeLists.txt outside my file set: the lead lands it on master or adds it to the set).
2. Put the pieces next to schema.cc as `src/database/schema_*.cc` (needs `src/database/schema_*`
   in the file set; no CMake change).
Recommendation: 1, the directory the quest names; I build on it meanwhile (a rename if 2).

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

## Cut plan

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
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | lead |

Jobs: 261121-200713-a4eb7a (fnsizes, Release+LTO at the base 57496b89a).

## Result

<When ready for review.>

## Rulings I made

- schema.h stays whole: 552-1068 is one class body (can't be cut) and 1-550 the enums and
  specification structs it is built on; no caller needs one without the other.
