---
title: Null type
---

### Check Null type


{% comment %}

```json
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

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
---
description: Index Null
---


```json
GET /test/schemas/implicit-_type/doc
```

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
---
description: Get Null
---

{% endcomment %}
