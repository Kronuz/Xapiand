---
description: UUID Field
---

### UUID Index Default

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
description: Index uuid field with default
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
description: Get schema uuid field with default
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
  field_name = '\u0001P\ufffd\ufffd\u000bkvH\'\ufffd\u0005\ufffd^\ufffd\ufffd\ufffdSsecond';
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  field_name =  '\u0001\ufffd=_\ufffd)\ufffdN\ufffd\ufffd_\ufffdA]\u0016mSthird';
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  field_name = '\u0001\ufffd\ufffdR\ufffd\ufffd\ufffdC\ufffd\ufffdq\u07357]lSfirst';
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '296138942', '1407656467', '3856745852']);
});
```
---
description: Info uuid field with default
---
{% endcomment %}


### UUID Index uuid_field

{% comment %}
```json
PUT /test/uuid/uuid_field/doc

{
  "uuids": {
    "_index_uuid_field": "uuid_field",
    "cfe052b3-c9d6-43b5-b971-7fdcb5375d6c": "first",
    "50ffd30b-6b76-4827-b805-f37f5e95f093": "second",
    "b63d5fc1-29ff-4ef0-835f-b5415d7f166d": "third"
  }
}
```
---
description: Index uuid field with uuid_field
---

```json
GET /test/uuid/uuid_field/
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
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_slot']).to.equal(2761136387);
});
```
---
description: Get schema uuid field with uuid_field
---

```json
INFO /test/uuid/uuid_field/doc
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
  pm.expect(jsonData.terms.uuids['<uuid_field>']).to.have.property('Sfirst');
  pm.expect(jsonData.terms.uuids['<uuid_field>']).to.have.property('Ssecond');
  pm.expect(jsonData.terms.uuids['<uuid_field>']).to.have.property('Sthird');
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '2761136387']);
});
```
---
description: Info uuid field with uuid_field
---
{% endcomment %}


### UUID Index Field Both

{% comment %}
```json
PUT /test/uuid/both/doc

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
GET /test/uuid/both/
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
  pm.expect(jsonData._schema.schema.uuids['<uuid_field>']['_slot']).to.equal(2761136387);
});
```
---
description: Get schema uuid field with both
---

```json
INFO /test/uuid/both/doc
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
  field_name = '\u0001P\ufffd\ufffd\u000bkvH\'\ufffd\u0005\ufffd^\ufffd\ufffd\ufffdSsecond';
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  field_name = '\u0001\ufffd=_\ufffd)\ufffdN\ufffd\ufffd_\ufffdA]\u0016mSthird';
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  field_name = '\u0001\ufffd\ufffdR\ufffd\ufffd\ufffdC\ufffd\ufffdq\u07357]lSfirst';
  pm.expect(jsonData.terms.uuids).to.have.property(field_name);
  pm.expect(jsonData.terms.uuids['<uuid_field>']).to.have.property('Sfirst');
  pm.expect(jsonData.terms.uuids['<uuid_field>']).to.have.property('Ssecond');
  pm.expect(jsonData.terms.uuids['<uuid_field>']).to.have.property('Sthird');
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '296138942', '1407656467', '2761136387', '3856745852']);
});
```
---
description: Info uuid field with both
---
{% endcomment %}