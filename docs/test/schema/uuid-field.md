---
description: UUID Field
---

### UUID Default

{% comment %}
```json
PUT /test/uuid/uuid/doc

{
  "uuids": {
    "cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first",
    "50ffd30b-6b76-4827-b805-f37f5e95f093": "second",
    "b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third"
  }
}
```
---
description: Index uuid field with default value
---

```json
GET /test/uuid/uuid/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.uuids).to.have.any.keys(['<uuid_field>']);
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_prefix']).to.equal('<uuid_field>.');
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_type']).to.equal('text');
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_slot']).to.equal(296138942);
});
```
---
description: Schema uuid field value is valid
---
{% endcomment %}

### UUID Default Index Field Both

{% comment %}
```json
PUT /test/uuid/uuid/doc

{
  "uuids": {
    "_index_uuid_field": "both",
    "cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first",
    "50ffd30b-6b76-4827-b805-f37f5e95f093": "second",
    "b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third",
  }
}
```
---
description: Index uuid field with both
---

```json
GET /test/uuid/uuid/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.uuids).to.have.any.keys(['<uuid_field>']);
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_prefix']).to.equal('<uuid_field>.');
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_type']).to.equal('text');
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_slot']).to.equal(296138942);
});
```
---
description: Get schema uuid field with both
---

```json
INFO /test/uuid/uuid/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  var field_name = '';
  pm.expect(jsonData.terms).to.have.property('QKdoc');
  field_name = String.fromCharCode(86) + String.fromCharCode(1) + String.fromCharCode(65533) + String.fromCharCode(65533);
  pm.expect(jsonData.terms).to.have.property(field_name);
  field_name = String.fromCharCode(1) + String.fromCharCode(80) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(11) + String.fromCharCode(107) + String.fromCharCode(118) + String.fromCharCode(72) + String.fromCharCode(39) + String.fromCharCode(65533) + String.fromCharCode(5) + String.fromCharCode(65533) + String.fromCharCode(127) + String.fromCharCode(94) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(83) + String.fromCharCode(115) + String.fromCharCode(101) + String.fromCharCode(99) + String.fromCharCode(111) + String.fromCharCode(110) + String.fromCharCode(100);
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  field_name = String.fromCharCode(1) + String.fromCharCode(65533) + String.fromCharCode(61) + String.fromCharCode(95) + String.fromCharCode(65533) + String.fromCharCode(41) + String.fromCharCode(65533) + String.fromCharCode(78) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(95) + String.fromCharCode(65533) + String.fromCharCode(65) + String.fromCharCode(93) + String.fromCharCode(  127) + String.fromCharCode(22) + String.fromCharCode(109) + String.fromCharCode(83) + String.fromCharCode(116) + String.fromCharCode(104) + String.fromCharCode(105) + String.fromCharCode(114) + String.fromCharCode(100);
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  field_name = String.fromCharCode(1) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(82) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(67) + String.fromCharCode(65533) + String.fromCharCode(65533) + String.fromCharCode(113) + String.fromCharCode(127) + String.fromCharCode(1845) + String.fromCharCode(55) + String.fromCharCode(93) + String.fromCharCode(108) + String.fromCharCode(83) + String.fromCharCode(102) + String.fromCharCode(105) + String.fromCharCode(114) + String.fromCharCode(115) + String.fromCharCode(116);
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '296138942', '1407656467', '3856745852']);
});
```
---
description: Info uuid field with both
---
{% endcomment %}