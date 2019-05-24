---
title: Min Aggregation
short_title: Min
---

A _single-value_ metrics aggregation that keeps track and returns the minimum
value among numeric values extracted from the aggregated documents. These
values are extracted from specific numeric fields in the documents.


## Structuring

The following snippet captures the structure of min aggregations:

```json
"<aggregation_name>": {
  "_min": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned minimum value.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section, computing the min balance value across all accounts:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "min_balance": {
      "_min": {
        "_field": "balance"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .test }

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

{: .test }

```js
pm.test("Response is aggregation", function() {
  var jsonData = pm.response.json();
  function expectEqualNumbers(a, b) {
    pm.expect(Math.round(parseFloat(a) * 1000) / 1000).to.equal(Math.round(parseFloat(b) * 1000) / 1000);
  }
  expectEqualNumbers(jsonData.aggregations._doc_count, 1000);
  expectEqualNumbers(jsonData.aggregations.min_balance._min, 7.99);
});
```

Response:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "min_balance": {
      "_min": 7.99
    }
  }, ...
}
```

As can be seen, the name of the aggregation (`min_balance` above) also serves as
the key by which the aggregation result can be retrieved from the returned
response.
