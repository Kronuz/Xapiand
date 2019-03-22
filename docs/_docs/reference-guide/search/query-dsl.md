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


## [Leaf Query Clauses](leaf-queries)

Leaf query clauses look for a particular value in a particular field, such as
the match, term or range queries. These queries can be used by themselves.


## [Compound Query Clauses](compound-queries)

Compound query clauses wrap other leaf or compound queries and are used to
combine multiple queries in a logical fashion (such as boolean query), or to
alter their behaviour.
