---
title: Query DSL
---

Xapiand provides a full Query DSL (Domain Specific Language) based on JSON to
define queries. Think of the Query DSL as an AST (Abstract Syntax Tree) of
queries.


## Match All Query

The simplest query, which matches all documents, returns all documents in any
given database giving them all a weight of 0.0:

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": {
    "_match_all": {}
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Match None Query

This is the inverse of the `_match_all` query, which matches no documents.

{% capture req %}

```json
POST /bank/:search?pretty

{
  "_query": {
    "_match_none": {}
  }
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
