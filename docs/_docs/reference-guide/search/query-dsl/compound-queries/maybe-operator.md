---
title: Maybe Operator
---

In addition to the basic logical operators, there is an additional logical
operator `_and_maybe` which matches any document which matches A (whether or
not B matches). If only B matches, then `_and_maybe` doesn't match. For this
operator, the weight is the sum of the matching subqueries, so:

* `_and_maybe`    - Finds any document which matches the first element of the
                    array and whether or not matches the rest.

1. Documents which match A and B match with the weight of A+B
2. Documents which match A only match with weight of A

This allows you to state that you require some terms (A) and that other
terms (B) are useful but not required.

#### Example

For example, the following matches all of those who like _bananas_ and which
maybe are also are _brown-eyed females_. It will return _brown-eyed females_
who like _bananas_ first:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "_and_maybe": [
      {
        "favoriteFruit": "banana"
      },
      {
        "_and": [
            { "gender": "female" },
            { "eyeColor": "brown" }
        ]
      }
    ]
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
pm.test("Maybe Operator count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

{: .test }

```js
pm.test("Maybe Operator size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

{: .test }

```js
pm.test("Maybe Operator values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [665, 319, 735, 475, 934, 969, 284, 576, 999, 417];
  for (var i = 0; i < 10; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
