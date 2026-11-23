# Schema source layout

`Schema` (declared in `src/database/schema.h`, the one include callers name) is implemented in
`src/database/schema.cc` and the pieces under `src/database/schema/`. Each piece opens with a comment
saying what it holds. Every function body is the one `schema.cc` had; the split moved code and
changed none (see "Linkage" for the few declarations that changed).

## Files

| File | Lines | Holds |
|---|---|---|
| `schema.cc` | 505 | the Schema object: constructor, initial schema, property access, `clear`, the readable form (`get_full`, `dispatch_readable`, `readable_*`), the stop-word checks, the walk outline |
| `schema/specification.cc` | 1,399 | `required_spc_t`, `index_spc_t`, `specification_t`, `default_spc`, and the tables they are built from: `_get_type`, the default and global accuracies, the acceptable-value lists |
| `schema/reserved.h` | 1,006 | the reserved-word vocabulary every piece uses, in `schema_detail`: type-name strings, parsers and printers of reserved values (index, stop and stem strategies, accuracies, uuid-field index, types, stem languages), `repr_field`, and the extern tables `specification.cc` defines |
| `schema/walk.h` | 419 | the inline members the walks share: `restart_specification`, the feed cache and `feed_subproperties`, `set_type_to_object`/`array`, `update_prefixes`, `detect_dynamic`, the `_id`/`_version` defaults; and `Schema::check` |
| `schema/dispatch.h` | 945 | the reserved-property dispatch: `dispatch_process_*`, `dispatch_write_*`, and the perfect-hash tables routing a reserved word to its process, write or feed handler, with the `has_dispatch_*` key sets |
| `schema/handlers.h` | 1,022 | the handlers the tables inline, run for a concrete field on every document: `process_*` for concrete properties, `consistency_*` for properties fixed once, `write_script`/`normalize_script` |
| `schema/index.cc` | 886 | `Schema::index`: the walk of a document, field by field (`index_subproperties`, `index_object`, `index_array`, `index_fields`, ...) |
| `schema/update.cc` | 1,039 | `Schema::update` and `Schema::write`: the two walks of a schema object |
| `schema/types.cc` | 748 | a field's type from its first value: `complete_specification`, `validate_required_data`, `validate_required_namespace_data`, `guess_concrete_type` |
| `schema/value.cc` | 731 | indexing one value: `index_item_value`, `index_item` (the per-type value indexers), `index_term`, partial paths, `store_item`, geospatial merges |
| `schema/properties.cc` | 883 | the handlers the tables call out of line: `write_*`, `process_*` for fields not concrete yet, the `_id`/`_version` defaults, `add_field` |
| `schema/feed.cc` | 703 | `dispatch_feed_properties` and the `feed_*` handlers: a stored schema's properties read into the specification |
| `schema/fields.cc` | 756 | what queries ask: `get_data_field`, `get_slot_field`, `get_dynamic_subproperties`, `get_data_id`, `get_prefixed_global`, the accuracy unit names `query_dsl` reads |

## Why headers hold code

Many members are defined `inline` in what was one translation unit, and the walks call them:
the dispatch tables are inlined into the walks, and the handlers into the tables. An inline
function must be defined in every translation unit that calls it, so these live in headers, moved
verbatim, rather than losing `inline` (which would change what the compiler inlines on the
indexing path). `walk.h`, `dispatch.h` and `handlers.h` are those headers; `dispatch.h` includes
`handlers.h`. `index.cc`, `update.cc`, `properties.cc` and `feed.cc` include `dispatch.h`.

`Schema::check` is a template that the constructor (`schema.cc`) and the update and write walks
instantiate with different error types, so its definition is in `walk.h`, where both see it.

## Kept whole

- `schema.h` (1,068 lines): 516 of them are the `Schema` class body, which can't be cut, and the
  rest are the enums and specification structs the class is built on.
- `schema/specification.cc` (1,399): `_get_type` is one 596-line perfect-hash table of type names;
  with the three specification structs' members and their tables it is one unit.
- `schema/handlers.h` (1,022): the 26 `consistency_*` checks and the 15 concrete-field `process_*`
  handlers are one family, each the per-document handler of one reserved word.
- `schema/reserved.h` (1,006): `_get_str_type` (267 lines) and `_get_stem_language` (230) are tables;
  the rest are their siblings for the other reserved values.
- `schema/update.cc` (1,039): the update and write walks mirror each other function by function;
  a fix to one usually applies to the other.

## Linkage

Changes beside the moves, all to share code across the pieces:

- The helpers in `reserved.h` were `static` (or plain `inline`) in `schema.cc`; they are `inline`
  in namespace `schema_detail`, and each piece says `using namespace schema_detail;`.
  `_get_stemmer_name` and `repr_field` gained `inline`.
- The acceptable-value lists (`str_set_*`) and default accuracies (`def_accuracy_num`, `_datetime`,
  `_time`, `_geo`) lost `static`: `specification.cc` defines them, `reserved.h` declares them. Their
  dynamic initializers stay in one translation unit with what they depend on (`global_acc_prefix_*`,
  `default_spc`).
- `normalize_script` gained `inline`: it shares `write_script`'s `#ifdef XAPIAND_LUA` in `handlers.h`.

## Adding a reserved word

1. The name in `reserved/schema.h`, and its field in `specification_t` (`schema.h`).
2. Its key in the tables in `schema/dispatch.h` that should handle it (and the matching
   `has_dispatch_*` key set).
3. Its handlers: `feed_*` in `schema/feed.cc`; `write_*` and first-time `process_*` in
   `schema/properties.cc`; a concrete-field `process_*` or `consistency_*` in `schema/handlers.h`;
   its readable form in `schema.cc` if it has one.
