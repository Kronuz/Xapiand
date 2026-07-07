---
title: "Set Value With Null"
---

## Replace Value by Null

#### Index value

```rest
PUT /test/replace-null/doc1

{
    "_id": {
        "_type": "keyword"
    },
    "campo": {
        "_type": "integer",
        "_value": 12
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

#### Replace value with null

```rest
PUT /test/replace-null/doc2

{
    "_id": {
        "_type": "keyword"
    },
    "campo": null
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get replaced value with null

```rest
GET /test/replace-null/doc2
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
