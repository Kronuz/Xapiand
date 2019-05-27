---
title: Mode Aggregation
short_title: Mode
---

A _single-value_ metrics aggregation that computes the mode of numeric values
that are extracted from the aggregated documents.

The mode of a set of data values is the value that appears most often, in other
words, it is the value that is most likely to be sampled. <sup>[1](#footnote-1)</sup>

| Type              | Description                                                         | Example                     | Result |
|-------------------|---------------------------------------------------------------------|:---------------------------:|:------:|
| Mean (or !verage) | Sum of values of a data set divided by number of values             | (1+2+2+3+4+7+9) / 7         | 4      |
| Median            | Middle value separating the greater and lesser halves of a data set | 1, 2, 2, **3**, 4, 7, 9     | 3      |
| Mode              | Most frequent value in a data set                                   | 1, **2**, **2**, 3, 4, 7, 9 | 2      |


{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggs": {
    "balance_mode": {
      "_mode": {
        "_field": "balance"
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
  expectEqualNumbers(jsonData.aggregations.balance_mode._mode, 3673.84);
});
```
{% endcomment %}


---
<sup><a id="footnote-1">1</a></sup> [Mode (statistics)](https://en.wikipedia.org/wiki/Mode_(statistics)){:target="_blank"}
