---
title: "Default Operator"
---

The default operator for text queries can be set by using the desired modifier.

## OR

_OR_ uses the `_or` modifier (the default).

## AND

_AND_ uses the `_and` modifier.

### Example

To make **AND** the default operator and thus forcing a query to search for
**all** terms instead of _any_ term (which is the default):

```rest
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_and": "these days are few and far between"
    }
  }
}
```

<!-- e2e:begin
---
params: sort=_id
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Default operator count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("Default operator size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("Default operator values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [2, 4, 8, 9, 11, 15, 16, 26, 27, 29];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->
