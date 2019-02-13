---
title: Query DSL
---

Xapiand provides a full Query DSL (Domain Specific Language) based on JSON to
define queries. Think of the Query DSL as an AST (Abstract Syntax Tree) of
queries.

Queries within Xapiand are the mechanism by which documents are searched for
within a database. They can be a simple search for text-based terms or a search
based on the values assigned to documents, which can be combined using a number
of different methods to produce more complex queries.


## Leaf Query Clauses

Leaf query clauses look for a particular value in a particular field, such as
the match, term or range queries. These queries can be used by themselves.

* [**_Match All Query_**](leaf-queries/match-all-query)
* [**_Simple Queries_**](leaf-queries/simple-queries)
* [**_Text Queries_**](leaf-queries/text-queries)
* [**_Namespace Queries_**](leaf-queries/namespace-queries)


## Compound Query Clauses

Compound query clauses wrap other leaf or compound queries and are used to
combine multiple queries in a logical fashion (such as boolean query), or to
alter their behaviour.

* [**_Logical Operators_**](compound-queries/logical-operators)
* [**_Maybe Operator_**](compound-queries/maybe-operator)
* [**_Filter Operator_**](compound-queries/filter-operator)
* [**_Elite Set Operator_**](compound-queries/elite-set-operator)
* [**_Range Searches_**](compound-queries/range-searches)
* [**_Geospatial Searches_**](compound-queries/geospatial-searches)


<!--
* `_max`          - Pick the maximum weight of any subquery. This matches the
                    same documents as a `_or`, but the weight contributed is
                    the maximum weight from any matching subquery (for `_or`,
                    it's the sum of the weights from the matching subqueries).
* `_wildcard`     - Wildcard expansion.
* `_scale_weight` -
* `_synonym`      -
 -->

<!--
## [Query Modifiers](query-modifiers)
The way simple queries are parsed can be adjusted using .


* [**_Text Queries_**](text-queries)
* [**_Namespace Queries_**](namespace-queries)
* [**_Cast Queries_**](cast-queries)
* [**_Geospatial Queries_**](geo-queries)
 -->
