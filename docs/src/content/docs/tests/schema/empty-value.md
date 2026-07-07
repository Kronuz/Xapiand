---
title: "Empty value"
---

## Field With Empty Value

#### Index Document With Empty Value

```rest
PUT /test/empty-field/doc

{
  "field": {
    "_type": "keyword",
    "_index": "field_all"
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

####  Get Document With Empty Value

```rest
GET /test/empty-field/doc
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
  pm.expect(jsonData._id).to.equal('doc');
});
```
e2e:end -->
