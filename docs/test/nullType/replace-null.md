---
title: Set Value With Null
---

### Replace Value by Null

{% comment %}

```json
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
---
description: Index value
---


```json
PUT /test/replace-null/doc2

{
    "_id": {
        "_type": "keyword"
    },
    "campo": null
}
```
---
description: Replace value with null
---


```json
GET /test/replace-null/doc2
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
description: Get replaced value with null
---

{% endcomment %}
