---
title: "Query DSL"
---

Xapiand provides a full Query DSL (Domain Specific Language) based on JSON to
define queries. Think of the Query DSL as an AST (Abstract Syntax Tree) of
queries.

Queries within Xapiand are the mechanism by which documents are searched for
within a database. They can be a simple search for text-based terms or a search
based on the values assigned to documents, which can be combined using a number
of different methods to produce more complex queries.

## [Leaf Query Clauses](leaf-queries)

Leaf query clauses look for a particular value in a particular field, such as
a field with a value (`"field": "value"`), a range (`_in` with `_range`), `_partial`,
`_wildcard` or `_fuzzy`, or `_match_all`. These queries can be used by themselves.

## [Compound Query Clauses](compound-queries)

Compound query clauses wrap other leaf or compound queries and are used to
combine multiple queries in a logical fashion (such as boolean query), or to
alter their behaviour.

## Limits

A query is at most 100 levels deep and 4,096 clauses, and an AND or FILTER joins at most 256
phrase or near groups; a larger query answers `400`. A request body nests at most 200 levels
(see [Limits](/Xapiand/reference-guide/api#limits)).
Sources: `src/engine/query.h:94-98`.
