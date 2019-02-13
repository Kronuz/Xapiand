---
title: Elite Set Operator
---

Picks the best _N_ subqueries and combine them with `_or`.

If you want to find documents similar to a piece of text, an obvious approach
is to build an `_or` query from all the terms in the text, and run this query
against a database containing the documents. However such a query can contain
lots of terms and be quite slow to perform, yet many of these terms don't
contribute usefully to the results.

The `_elite_set` operator can be used instead of `_or` in this situation.
`_elite_set` selects the **most important** _N_ terms and then acts as an `_or`
query with just these, ignoring any other terms. This will usually return
results just as good as the full `_or` query, but much faster.

In general, the `_elite_set` operator can be used when you have a large OR
query, but it doesn't matter if the search completely ignores some of the less
important terms in the query.

The subqueries don't have to be terms, but if they aren't then `_elite_set`
will look at the estimated frequencies of the subqueries and so could pick a
subset which don't actually match any documents even if the full OR would
match some.

You can specify a parameter to the query constructor which control the number
of terms which `_elite_set` will pick. If not specified, this defaults to _10_.

If the number of subqueries is less than this threshold, `_elite_set`
behaves identically to `_or`.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_elite_set": [
      { "personality": "biggest" },
      { "personality": "things" },
      { "personality": "lovable" }
      { "personality": "cooperative" },
      { "personality": "course" },
      { "personality": "also" },
      { "personality": "kind" },
      { "personality": "honest" },
      { "personality": "considerate" },
      { "personality": "prominent" },
      { "personality": "especially" },
      { "personality": "compared" },
      { "personality": "impulses" },
      { "personality": "shallow" },
      { "personality": "well" }
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

A similar result can be achieved by setting the _default operator_ via the
[Elite Set Modifier](../query-modifiers#elite-set-modifier).
