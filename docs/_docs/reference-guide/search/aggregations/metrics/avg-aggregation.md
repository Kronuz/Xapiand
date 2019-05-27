---
title: Average Aggregation
short_title: Average
---

A _single-value_ metrics aggregation that computes the average of numeric values
that are extracted from the aggregated documents.

arithmetic mean, also called the mathematical expectation or average, is the
central value of a discrete set of numbers: specifically, the sum of the values
divided by the number of values. <sup>[1](#footnote-1)</sup>

| Type              | Description                                                         | Example                     | Result |
|-------------------|---------------------------------------------------------------------|:---------------------------:|:------:|
| Mean (or Average) | Sum of values of a data set divided by number of values             | (1+2+2+3+4+7+9) / 7         | 4      |
| Median            | Middle value separating the greater and lesser halves of a data set | 1, 2, 2, **3**, 4, 7, 9     | 3      |
| Mode              | Most frequent value in a data set                                   | 1, **2**, **2**, 3, 4, 7, 9 | 2      |


## Structuring

The following snippet captures the structure of average aggregations:

```json
"<aggregation_name>": {
  "_avg": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned average.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section, computing the average age of all account holders:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "avg_age": {
      "_avg": {
        "_field": "age"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Response is aggregation", function() {
  var jsonData = pm.response.json();
  function expectEqualNumbers(a, b) {
    pm.expect(Math.round(parseFloat(a) * 1000) / 1000).to.equal(Math.round(parseFloat(b) * 1000) / 1000);
  }
  expectEqualNumbers(jsonData.aggregations._doc_count, 1000);
  expectEqualNumbers(jsonData.aggregations.avg_age._avg, 30.042);
});
```
{% endcomment %}

The above aggregation computes the average grade over all documents. The above
will return the following:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "avg_age": {
      "_avg": 30.042
    }
  }, ...
}
```

The name of the aggregation (`avg_grade` above) also serves as the key by which
the aggregation result can be retrieved from the returned response.


---
<sup><a id="footnote-1">1</a></sup> [Mean](https://en.wikipedia.org/wiki/Mean){:target="_blank"}
