> Status: CURRENT (2026-10-28) describes `phase7a/seam-fields` (X6); not landed.

# Seam fields: how it works

A term at the engine seam is a field and the bytes under it, never one string with the field's
name glued to the front. `engine::Document` and the query nodes name the field; each engine
decides what a field is. The Xapian engine joins the two back into the term Xapiand has always
written, so its index files, answers and write-ahead log lines are unchanged. The strata engine
keeps terms apart by field and declares each field in the index's field table (SF,
`strata-fields.md`). Design: strata `docs/design/fields-and-keys.md` (D1, D9, D12, C4).

## How it works

```
Schema / GenerateTerms / QueryDSL            seam                         engine
 field_name(prefix, ctype)  ----------->  Document::field(name, kind) -> FieldRef (document-local)
 term bytes                 ----------->  add_term(FieldRef, term, wdf)  -> Xapian: name + term
                                                                          -> strata: DocField(name).terms
 field_name(prefix, ctype)  ----------->  Query::term(field, term, ..)   -> Xapian: Query(name + term)
```

1. **Field names.** An engine field is a Xapiand (prefix, type character) pair and its name is
   those bytes (`field_name`, `src/database/utils.cc`): `name.firstName.S`, `<186a0>.N` for an
   accuracy level, `checkin.<a>.G` for an HTM level, the bare prefix `name.` for the null marker
   (with the empty term), `Zname.firstName.S` for the stems of a text field. The name is any
   bytes; the engine never reads structure in it.
2. **Documents.** `Document::field(name, kind)` returns a document-local `FieldRef`; asking again
   returns the same one and can only widen the kind (`boolean` < `terms` < `text`: documents only,
   frequencies, positions). The table lives in `Document`, so both engines share one
   implementation, and `Document::Native` receives the `Field` (name and kind) with each call.
   `set_key(key)` is unchanged: the key is the document's id term, an opaque string built by
   `flat_term` (`database/utils.h`), which is also what shard placement hashes.
3. **Queries.** `Query::term`, `wildcard` and `edit_distance` take the field apart from the
   term or pattern; `Query::field(name)` is any term in the field; `TextConfig::field` is the
   field the parser reads. An empty field with an empty term is match-all, and a field with an
   empty term is the field's marker term. The edit distance's `prefix_length` counts from the
   start of the term; the Xapian engine adds the field's length.
4. **Text.** The term generator runs with no prefix. A term that leads with `Z` is a stem (words
   are lowercase, so a plain word never does) and goes to the field `Z` + name; the rest go to
   the field. This stands in for `Kronuz/analysis` reporting stems itself (an additive change
   D12 names for SF); `test_text_fields` compares every stem strategy against the generator run
   with the prefix.
5. **The strata engine.** `StrataDocument` keeps the terms by field (`strata-fields.md`). Nothing else in
   the engine joins a field and a term.

## Invariants

1. A document built through fields serialises to the bytes of the Xapian document of the joined
   terms (`test_xapian_terms_are_joined`, random operations against Xapian calls).
2. A query node translates to the Xapian query of the joined strings, edit-distance prefix
   included (`test_query_nodes`).
3. A query on a field finds the documents with that field's term and no others
   (`test_field_selects_terms`: a mutant that drops or swaps a prefix fails it).
4. Text and stems land in the same Xapian terms as before for every strategy
   (`test_text_fields`, `test_word_break_fields`).
5. The Xapian engine cannot tell `("ab", "c")` from `("a", "bc")`: both are the term `abc`
   (`test_marker_and_joined_ambiguity`). Strata keeps them apart once SF lands.

## Not covered here

1. `DocView::terms(prefix, sink)` and `term_details` still report whole Xapian terms.
2. Shard key operations (`replace_by_key`, `find`, `erase_by_key`) take the opaque key string.
3. `Query::field("name.")` reads the name as a prefix on the Xapian engine, so `field: "*"` over an
   object reaches nested fields as before. SF replaces it with an OR over the schema's fields.
4. The 245-byte term limit is checked in `Schema::index_term` on the joined length.

## Code

`src/engine/document.{h,cc}`, `src/engine/query.{h,cc}`, `src/engine/xapian/{convert,query,text}.cc`,
`src/engine/strata/document.cc`, `src/database/{utils,schema}.cc`,
`src/multivalue/generate_terms.cc`, `src/query_dsl.cc`; tests `tests/test_engine_fields.cc`.
Ledger: `docs/status/seam-fields.md`.
