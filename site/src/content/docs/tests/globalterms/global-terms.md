---
title: "Global Terms"
---

## Search For Global Term

#### Index for Global

```rest
PUT /test/doc

{
  "_id": {
    "_type": "keyword",
  },
  "ident": {
    "_type": "uuid",
    "_index": "global_terms",
    "_value": "~notmet"
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

####  Search for Global

```rest
SEARCH /test/
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits[0].ident).to.equal('~notmet');
});
```
e2e:end -->
