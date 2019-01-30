---
title: Query DSL
---

Xapiand provides a full Query DSL (Domain Specific Language) based on JSON to define queries. Think of the Query DSL as an AST (Abstract Syntax Tree) of queries.

## Leaf query clauses

Leaf query clauses look for a particular value in a particular field, such as the match, term or range queries. These queries can be used by themselves.

## Compound query clauses

Compound query clauses wrap other leaf or compound queries and are used to combine multiple queries in a logical fashion (such as boolean query), or to alter their behaviour.

* [**_Queries_**](query-dsl)
* [**_Text Queries_**](text-queries)