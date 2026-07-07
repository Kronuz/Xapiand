---
title: "Filter Aggregation"
---

Defines a _single-bucket_ of all the documents in the current document set
context that match a specified filter.

## Structuring

The following snippet captures the structure of filter aggregations:

```json
"<aggregation_name>": {
  "_filter": {
    "_term": {
      ( "<key>": <value>, )*
    }
  },
  ...
}
```

Also supports all other functionality as explained in [Bucket Aggregations](..#structuring).

### Filtering Terms

Often this will be used to narrow down the current aggregation context to a
specific set of documents containing certain terms.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration](/Xapiand/exploration#sample-dataset)
section:

```rest
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "strawberry_lovers": {
      "_filter": {
        "_term": {
          "favoriteFruit": "strawberry"
        }
      },
      "_aggs": {
        "avg_balance": {
          "_avg": {
            "_field": "balance"
          }
        }
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
  pm.expect(jsonData.aggregations.strawberry_lovers._doc_count).to.equal(76);
  pm.expect(jsonData.aggregations.strawberry_lovers.avg_balance._avg).to.be.closeTo(2581.807, 0.01);
});
```
e2e:end -->

In the above example, we calculate the average balance of all the bank accounts
with holders who fancy strawberries.

Response:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "strawberry_lovers": {
      "_doc_count": 76,
      "avg_balance": {
        "_avg": 2581.807236842105
      }
    }
  }, ...
}
```
