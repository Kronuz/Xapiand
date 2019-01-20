---
title: Average Aggregation
---

A _single-value_ metrics aggregation that computes the average of numeric values
that are extracted from the aggregated documents. These values are extracted
from specific numeric fields in the documents.

Assuming the data consists of documents representing exams grades (between 0
and 100) of students we can average their scores with:

{% capture req %}

```json
POST /exams/:search?pretty

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "avg_grade": {
      "_avg": {
      	"_field": "grade"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above aggregation computes the average grade over all documents. The
aggregation type is `_avg` and the `_field` setting defines the numeric field
of the documents the average will be computed on. The above will return the
following:

```json
{
    ...
    "#aggregations": {
        "avg_grade": {
            "value": 75.0
        }
    }
}
```

The name of the aggregation (`avg_grade` above) also serves as the key by which
the aggregation result can be retrieved from the returned response.
