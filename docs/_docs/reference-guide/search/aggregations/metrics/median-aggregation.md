---
title: Median Aggregation
short_title: Median
---

{: .note .construction }
_This section is a **work in progress**..._

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_median": {
      "_median": {
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
  expectEqualNumbers(jsonData.aggregations.balance_median._median, 2414.425);
});
```
