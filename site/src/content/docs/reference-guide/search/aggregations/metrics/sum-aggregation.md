---
title: "Sum Aggregation"
---

A _single-value_ metrics aggregation that sums up numeric values that are
extracted from the aggregated documents.

## Structuring

The following snippet captures the structure of sum aggregations:

```json
"<aggregation_name>": {
  "_sum": {
    "_field": "<field_name>"
  },
  ...
}
```

### Field

The `<field_name>` in the `_field` parameter defines the specific field from
which the numeric values in the documents are extracted.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration](/Xapiand/exploration#sample-dataset)
section, we can sum the balances of all accounts in the state of Indiana with:

```rest
SEARCH /bank/

{
  "_query": {
    "contact.state": "Indiana"
  },
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "indiana_total_balance": {
      "_sum": {
        "_field": "balance"
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
  expectEqualNumbers(jsonData.aggregations.indiana_total_balance._sum, 42152.87);
});
```
e2e:end -->

Resulting in:

```json
{
  "aggregations": {
    "_doc_count": 17,
    "indiana_total_balance": {
      "_sum": 42152.87
    }
  }, ...
}
```
