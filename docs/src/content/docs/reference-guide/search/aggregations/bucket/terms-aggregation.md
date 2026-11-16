---
title: "Terms Aggregation"
---

A _multi-bucket_ value source based aggregation where buckets are dynamically
built - one per unique **term**.

The terms of a `keyword`, numeric, `boolean` or `date` field are its stored values, and a
document counts once for each distinct value. An analysed `text` field has no stored values to
count, so aggregating on one answers `400`.

:::hint[Performance]{.caution}
`_terms` and [Values Aggregation](../values-aggregation) read the same stored values and cost the
same. The differences: `_terms` takes the type from the field's data, and refuses `text` fields and
fields with no values (`400`); a `keyword` keeps its case (`"Red"` and `"red"` are two buckets) and a
numeric field returns its values, not the accuracy terms; a `geospatial` field answers `400`.
:::

## Structuring

The following snippet captures the structure of terms aggregations:

```json
"<aggregation_name>": {
  "_terms": {
    "_field": "<field_name>"
  },
  ...
}
```

Also supports all other functionality as explained in [Bucket Aggregations](..#structuring).

### Field

The `<field_name>` in the `_field` parameter defines the field on which the
aggregation will act upon.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration](/Xapiand/exploration#sample-dataset)
section:

```rest
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggregations": {
    "ages": {
      "_terms": {
        "_field": "age"
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
  var expected_doc_count = [
    57, 56, 55, 55, 55, 54, 54, 51, 49, 48, 47, 46, 46, 45, 44, 41, 40, 38, 36, 31, 28, 5, 5, 4, 3, 3, 2, 2];
  var expected_key = [
    "38", "30", "39", "33", "24", "36", "31", "26", "21", "29", "32", "22", "20", "34", "25", "35", "27", "37", "23", "28", "40", "42", "19", "41", "18", "17", "44", "43"];
  for (var i = 0; i < expected_doc_count.length; ++i) {
    pm.expect(jsonData.aggregations.ages[i]._doc_count).to.equal(expected_doc_count[i]);
    pm.expect(jsonData.aggregations.ages[i]._key).to.equal(expected_key[i]);
  }
});
```
e2e:end -->

Response:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "ages": [
      ...
    ]
  }, ...
}
```

### Ordering

By default, the returned buckets are sorted by their `_doc_count` descending,
though the order behaviour can be controlled using the `_sort` setting. Supports
the same order functionality as explained in [Bucket Ordering](..#ordering).
