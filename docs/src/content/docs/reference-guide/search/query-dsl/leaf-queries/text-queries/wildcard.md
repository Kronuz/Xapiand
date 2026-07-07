---
title: "Wildcard Query"
---

Matches documents that have fields matching a wildcard expression. Supported
wildcards are `*`, which matches any character sequence (including the empty
one), and `?`, which matches any single character. Note that this query can be
slow, as it needs to iterate over many terms. In order to prevent extremely
slow wildcard queries, a wildcard term should not start with one of the
wildcards `*` or `?`.

This allows for prefix matches, matching any number of trailing characters, so,
for instance, `"wildc*"` would match _**wildc**ard_, _**wildc**arded_,
_**wildc**ards_, _**wildc**at_, _**wildc**ats_, etc.

:::hint{.tip}
This is a bit different from [Partial Query](../partial).
Partial is intended for "incremental search".
:::

### Example

```rest
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": {
      "_wildcard": "ba"
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
pm.test("wildcard query count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("wildcard query size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("wildcard query values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [77, 84, 120, 173, 234, 279, 280, 284, 289, 319];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->

A similar effect could be obtained by using the wildcard ("`*`") character
as part of the query text:

```rest
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": "ba*"
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
pm.test("wildcard suffix query count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("wildcard suffix query size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("wildcard suffix query values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [77, 84, 120, 173, 234, 279, 280, 284, 289, 319];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->
