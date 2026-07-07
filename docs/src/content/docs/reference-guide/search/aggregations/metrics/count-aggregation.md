---
title: "Count Aggregation"
---

A _single-value_ metrics aggregation that counts the number of values that are
extracted from the aggregated documents.

Typically, this aggregator will be used in conjunction with other single-value
aggregations. For example, when computing the `_avg` one might be interested in
the number of values the average is computed over.

## Structuring

The following snippet captures the structure of count aggregations:

```json
"<aggregation_name>": {
  "_count": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted and used to compute the
returned count.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration](/Xapiand/exploration#sample-dataset)
section, computing the number of cities with accounts in the state of Indiana:

```rest
SEARCH /bank/

{
  "_query": {
    "contact.state": "Indiana"
  },
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "indiana_city_count": {
      "_count": {
        "_field": "contact.city"
      }
    }
  }
}
```

<!-- e2e:begin

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
  expectEqualNumbers(jsonData.aggregations.indiana_city_count._count, 17);
});
```
e2e:end -->

Response:

```json
  "aggregations": {
    "_doc_count": 17,
    "indiana_city_count": {
      "_count": 17
    }
  }, ...
```
