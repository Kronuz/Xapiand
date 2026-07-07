---
title: "Ignore Comments"
---

## Ignore Comments

#### Ignore comment

```rest
POST /test/comment-ignore/

{
  "_recurse": false,
  "_id": {
    "_type": "uuid",
  },
  "#comment": "This comment is ignored",
  "_schema": {
    "_type": "object"
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  for (key in jsonData.schema) {
      pm.expect(key).to.not.include('#comment');
  }
});
```
e2e:end -->
