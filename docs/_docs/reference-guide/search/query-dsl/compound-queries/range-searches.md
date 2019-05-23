---
title: Range Searches
---

The keyword `_range` matches documents where the given value is between the
given `_from` and `_to` fixed range (including both endpoints).

If you only use the keyword `_from` matches documents where the given value is
greater than or equal to a fixed value.

If you only use the keyword `_to` matches documents where the given value is
less than or equal a fixed value.

#### Example

This example find _all_ bank accounts for which their account holders are
_females_ in the ages between 20 and 30:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "_and": [
      { "gender": "female" },
      {
        "age": {
          "_in": {
            "_range": {
              "_from": 20,
              "_to": 30
            }
          }
        }
      }
    ]
  },
  "_sort": "accountNumber"
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
pm.test("Range Searches count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

{: .test }

```js
pm.test("Range Searches size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

{: .test }

```js
pm.test("Range Searches values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [100123, 103710, 104419, 109766, 112573, 118960, 124835, 130906, 134887, 140681];
  for (var i = 0; i < 10; ++i) {
    pm.expect(jsonData.hits[i].accountNumber).to.equal(expected[i]);
  }
});
```
