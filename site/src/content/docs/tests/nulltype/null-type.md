---
title: "Null type"
---

## Check Null type

#### Index Null

```rest
PUT /test/schemas/implicit-_type/doc

{
    "_id": {
        "_type": "keyword",
    },
    "campo": {
        "_type": "keyword",
        "_value": null
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

#### Get Null

```rest
GET /test/schemas/implicit-_type/doc
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
  pm.expect(jsonData.campo).to.equal(null);
});
```
e2e:end -->
