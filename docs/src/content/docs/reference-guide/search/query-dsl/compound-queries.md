---
title: "Compound Query Clauses"
---

Compound query clauses wrap other leaf or compound queries and are used to
combine multiple queries in a logical fashion (such as boolean query), or to
alter their behaviour.

* [**_Logical Operators_**](logical-operators)
* [**_Maybe Operator_**](maybe-operator)
* [**_Filter Operator_**](filter-operator)
* [**_Range Searches_**](range-searches)
* [**_Geospatial Searches_**](geospatial-searches)

## Modifiers

`_max` and `_synonym` are modifiers of a text query, not compound clauses: `"field": {"_max": "a b"}`
takes the maximum weight of the words instead of their sum, and `_synonym` treats the words as
synonyms (see [Default Operator](/Xapiand/reference-guide/search/query-dsl/leaf-queries/text-queries/default-operator)).
Given a list of queries, `_scale_weight` and `_wildcard` answer `400` ("not a valid query").
Sources: `src/query_dsl.cc:335-340`, `src/engine/query.cc:101-111`.
