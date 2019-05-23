---
title: Logical Operators
---

Each query produces a list of documents with a weight according to how good a
match each document is for that query. These queries can then be combined to
produce a more complex tree-like query structure, with the operators acting as
branches within the tree.

The most basic operators are the logical operators: `_or`, `_and` and `_not` -
these match documents in the following way:

* `_or`           - Finds documents which match any of the subqueries.
* `_and`          - Finds documents which match all of the subqueries.
* `_not`          - Finds documents which don't match any of the subqueries.
* `_and_not`      - Finds documents which match the first subquery A but
                    not subquery B.
* `_xor`          - Finds documents which are matched by subquery A or other or
                    subquery B, but not both.

Each operator produces a weight for each document it matches, which depends on
the weight of one or both subqueries in the following way:

* `_or`           - Matches documents with the sum of all weights of the subqueries.
* `_and`          - Matches documents with the sum of all weights of the subqueries.
* `_not`          - Finds documents which don't match any of the subqueries.
* `_and_not`      - Matches documents with the weight from subquery A only.

#### Example

For example, the following matches all of those who either like _bananas_ or
are _brown-eyed females_:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "_or": [
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
pm.test("Logical Operator count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

{: .test }

```js
pm.test("Logical Operator size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

{: .test }

```js
pm.test("Logical Operator values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [100123, 100481, 101121, 103213, 112495, 112573, 113400, 114772, 117339, 119292];
  for (var i = 0; i < 10; ++i) {
    pm.expect(jsonData.hits[i].accountNumber).to.equal(expected[i]);
  }
});
```
