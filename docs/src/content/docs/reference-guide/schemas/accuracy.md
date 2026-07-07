---
title: "Accuracy"
---

Numeric, date and geospatial fields index extra terms at coarser granularities,
called _accuracies_. A range query can then match a handful of coarse "bucket"
terms instead of scanning every distinct value, which makes ranges and range
based aggregations much faster. The trade-off is a larger index: each accuracy
level adds terms.

Accuracies are set per field with the `_accuracy` option. If it is omitted a
sensible default is used, so you only need to tune it when you know your query
patterns.

## Numeric Accuracy

For numeric fields, `_accuracy` is an array of step sizes. The field below
buckets its values at multiples of 100, 1000 and 10000:

```rest
PUT /test_accuracy/

{
  "_schema": {
    "balance": {
      "_type": "float",
      "_accuracy": [100, 1000, 10000]
    }
  }
}
```

```rest
PUT /test_accuracy/1

{
  "balance": 1234.56
}
```

Ranges over the field use those buckets transparently, the results are exactly
the same as without accuracies, only faster:

```rest
SEARCH /test_accuracy/

{
  "_query": {
    "balance": {
      "_in": {
        "_range": { "_from": 1000, "_to": 2000 }
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
pm.test("Accuracy range query matches", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
e2e:end -->

When `_accuracy` is omitted, numeric fields default to
`[100, 1000, 10000, 100000, 1000000, 100000000]`.

## Date Accuracy

For date fields, `_accuracy` is an array of named levels instead of step sizes:
`"second"`, `"minute"`, `"hour"`, `"day"`, `"month"`, `"year"`, `"decade"`,
`"century"` and `"millennium"`.

See
[Date Type](/Xapiand/reference-guide/schemas/field-types/date-type)
and
[Numeric Type](/Xapiand/reference-guide/schemas/field-types/numeric-type)
for the per-type details.
