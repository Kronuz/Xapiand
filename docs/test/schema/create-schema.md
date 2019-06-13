---
title: Create Schema
---

### Create Simple Schema

{% comment %}
```json
PUT /test/create-schema/

{
  "_schema": {
    "schema": {
      "name": {
        "_type": "text"
      },
      "age": {
        "_type": "positive"
      }
    }
  }
}
```
---
description: Create schema
---

```json
GET /test/create-schema/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.name).to.include({'_type': 'text' });
  pm.expect(jsonData._schema.schema.age).to.include({'_type': 'positive' });
});
```
---
description: Schema values are valid
---
{% endcomment %}