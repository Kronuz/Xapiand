---
title: "Match All Query"
---

The simplest query is `_match_all`, which matches all documents, returns all
documents in any given database giving them all a weight of `0.0`:

#### Example

```rest
SEARCH /bank/

{
  "_query": {
    "_match_all": {}
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
pm.test("match all total", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1000);
});
```
e2e:end -->

# Match None Query

The query `_match_none` is the inverse of the `_match_all` query, and matches
no documents.

#### Example

```rest
SEARCH /bank/

{
  "_query": {
    "_match_none": {}
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
pm.test("match all total", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
});
```
e2e:end -->
