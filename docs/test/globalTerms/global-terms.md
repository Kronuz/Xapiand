---
title: Global Terms
---

### Search For Global Term

{% comment %}

```json
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

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

---
description: Index for Global
---

```json
SEARCH /test/
```

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

---
description: Search for Global
---

{% endcomment %}
