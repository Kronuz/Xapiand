---
title: "Near Query"
---

Another commonly used operator is `_near`, which finds terms near each other in the
current document, behaving like `_and` with regard to weights, so that:

* Documents which match A near B are matched, with weight of A+B

The window is 10 + the number of words - 1 positions (11 for two words); `NEAR/n` in a
text under `_and` or `_or` sets `n`. The operands must be plain words: a wildcard, a
fuzzy word or a group inside `_near` answers `400`.

#### Example

```rest
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_near": "adventurous ambitious"
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
pm.test("near query count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(2);
});
```

```js
pm.test("near query size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(2);
});
```

```js
pm.test("near query values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [282, 494];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->
