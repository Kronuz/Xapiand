---
title: Simple Query
---

This is the standard query for performing text queries, if you use `_language` keyword at the index time a stemming algorithm is used which is a process of linguistic normalisation, in which the variant forms of a word are reduced to a common form. This query example match with any document with connection in the text field personality but also match with connect because connection is reduced to connect by the stemmig algorithm. By default the `_language` is empty and in that case the stemming is not used.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "responsive"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Resulting in documents containing the word "_responsive_" in any part of the
personality field body.

We can see in the result that just one document contain the word connection in
the text and the other just the word connect in personality field and both
match due the stemming.
