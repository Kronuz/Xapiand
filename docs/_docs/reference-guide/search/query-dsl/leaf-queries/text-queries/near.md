---
title: Near Query
short_title: Near
---

Another commonly used operator is `_near`, which finds terms within 10 words
of each other in the current document, behaving like `_and` with regard to
weights, so that:

* Documents which match A within 10 words of B are matched, with weight of A+B

#### Example

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_near": "adventurous ambitious"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .test }

```js
pm.test("response is ok", function() {
  pm.response.to.be.ok;
});
```

{: .test }

```js
pm.test("near query count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(2);
});
```

{: .test }

```js
pm.test("near query size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(2);
});
```

{: .test }

```js
pm.test("near query values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [282, 494];
  for (var i = 0; i < 2; ++i) {
      pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
