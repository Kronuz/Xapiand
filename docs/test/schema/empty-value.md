---
title: Empty value
---

### Field With Empty Value

{% comment %}
```json
PUT /test/empty-field/doc

{
  "field": {
    "_type": "keyword",
    "_index": "field_all"
  }
}
```
---
description: Index Document With Empty Value
---

```json
GET /test/empty-field/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData).to.all.keys(['_id', '_version', '#docid', '#shard']);
});
```
---
description: Get Document With Empty Value
---

{% endcomment %}