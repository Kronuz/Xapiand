---
title: Documents like this
---

Pick the best N subqueries and combine with OR.

If you want to find documents similar to a piece of text, an obvious approach is to build an "OR" query from all the terms in the text, and run this query against a database containing the documents. However such a query can contain a lots of terms and be quite slow to perform, yet many of these terms don't contribute usefully to the results.

The `_elite_set` operator can be used instead of OR in this situation. `_elite_set` selects the most important ''N'' terms and then acts as an OR query with just these, ignoring any other terms. This will usually return results just as good as the full OR query, but much faster.

In general, the `_elite_set` operator can be used when you have a large OR query, but it doesn't matter if the search completely ignores some of the less important terms in the query.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_elite_set": {
      "personality": "the biggest two things to know are that hes lovable and cooperative. Of course hes also kind, honest and considerate, but theyre far less prominent, especially compared to impulses of being shallow as well"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}