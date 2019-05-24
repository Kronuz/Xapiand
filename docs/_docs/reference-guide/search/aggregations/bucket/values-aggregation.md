---
title: Values Aggregation
short_title: Values
---

A _multi-bucket_ value source based aggregation where buckets are dynamically
built - one per unique **value**.


## Structuring

The following snippet captures the structure of range aggregations:

```json
"<aggregation_name>": {
  "_values": {
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
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section, listing all favorite fruits of all account holders:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggregations": {
    "favorite_fruits": {
      "_values": {
        "_field": "favoriteFruit"
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
  var expected_doc_count = [89, 76, 73, 72, 64, 58, 57, 52, 49, 49, 43, 42, 41, 37, 36, 34, 32, 30, 29, 25];
  var expected_key = ["apple", "strawberry", "grape", "watermelon", "banana", "lemon", "orange", "pear", "blueberry", "avocado", "peach", "cherry", "pineapple", "cantaloupe", "lime", "raspberry", "blackberry", "plum", "grapefruit", "nectarine"];
  for (var i = 0; i < expected_doc_count.length; ++i) {
    pm.expect(jsonData.aggregations.favorite_fruits[i]._doc_count).to.equal(expected_doc_count[i]);
    pm.expect(jsonData.aggregations.favorite_fruits[i]._key).to.equal(expected_key[i]);
  }
});
```
{% endcomment %}

Response:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "favorite_fruits": [
      {
        "_doc_count": 76,
        "_key": "strawberry"
      },
      {
        "_doc_count": 64,
        "_key": "banana"
      },
      {
        "_doc_count": 89,
        "_key": "apple"
      }
    ]
  }, ...
}
```

### Ordering

By default, the returned buckets are sorted by their `_doc_count` descending,
though the order behaviour can be controlled using the `_sort` setting. Supports
the same order functionality as explained in [Bucket Ordering](..#ordering).
