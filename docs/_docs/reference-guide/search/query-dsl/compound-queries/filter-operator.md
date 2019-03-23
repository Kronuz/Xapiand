---
title: Filter Operator
---

A query can be filtered by another query. There are two ways to apply a filter
to a query depending whether you want to include or exclude documents:

* `_filter`       - Matches documents which match both subqueries, but the
                    weight is only taken from the left subquery (in other
                    respects it acts like `_and`.
* `_and_not`      - Matches documents which match the left subquery but don’t
                    match the right hand one (with weights coming from the left
                    subquery)

#### Example

For example, the following matches all who like _bananas_ filtering the results
to those who also are _brown-eyed females_, but this filter doesn't affect
weights:

{% capture req %}

```json
GET /bank/:search

{
  "_query": {
    "_filter": [
      {
        "favoriteFruit": "banana"
      },
      {
        "_and": [
            { "gender": "female" },
            { "eyeColor": "brown" }
        ]
      }
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
