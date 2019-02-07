---
title: Query DSL
---

Xapiand provides a full Query DSL (Domain Specific Language) based on JSON to
define queries. Think of the Query DSL as an AST (Abstract Syntax Tree) of
queries.

## Match All Query

The simplest query, which matches all documents, returns all documents in any
given database:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": "*"
}
```
{% endcapture %}
{% include curl.html req=req %}


## Leaf query clauses

Leaf query clauses look for a particular value in a particular field, such as
the match, term or range queries. These queries can be used by themselves.


## Compound query clauses

Compound query clauses wrap other leaf or compound queries and are used to
combine multiple queries in a logical fashion (such as boolean query), or to
alter their behaviour.

Query clauses behave differently depending on whether they are used in query
context or filter context.

* [**_Basic Queries_**](basic-queries)
* [**_Text Queries_**](text-queries)
* [**_Namespace Queries_**](namespace-queries)
* [**_Cast Queries_**](cast-queries)
* [**_Geospatial Queries_**](geo-queries)
* [**_Find Documents Like This_**](like-this-queries)
* [**_Near Queries_**](near-queries)
