---
title: "Restore"
---

## Restore

#### Restore documents

```rest
RESTORE /test/restore/
Content-Type: application/x-ndjson

@fruits.ndjson
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Values are valid", function() {
 var jsonData = pm.response.json();
 var tmp = {};
 for (var i=0; i < jsonData.items.length; ++i) {
  pm.expect(tmp.hasOwnProperty(jsonData.items[i]._id)).to.be.false;
  if (!tmp.hasOwnProperty(jsonData.items[i]._id)) {
    tmp[jsonData.items[i]._id] = 1;
  }
 }
});
```
e2e:end -->
