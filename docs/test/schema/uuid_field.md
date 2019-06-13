---
description: Schema With UUID Field
---

### Create Schema With UUID Field

{% comment %}
```json
PUT /test/create-schema-uuid-field/doc

{
  "_schema": {
    "schema": {
      "_id": {
      "_type": "keyword",
      },
      "<uuid_field>": {
        "meta": {
          "_recurse": false,
        },
        "roles": {
          "_type": "array/keyword",
        }
      }
    }
  },
  "~3pZyPcFqGq": {
    "meta": {
      "scope": "read write"
    },
    "roles": [
      "myself"
    ]
  }
}
```
---
description: Create schema uuid field
---

```json
GET /test/create-schema-uuid-field/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema).to.have.any.keys(['<uuid_field>']);
});
```
---
description: Schema uuid field value is valid
---
{% endcomment %}
