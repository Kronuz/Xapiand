---
title: Define Slot
---

### Index document with _slot

{% comment %}
```json
PUT /test/define-slot/doc

{
  "foo": {
    "_type": "string",
    "_value": "foo and bar",
    "_index": "field_values",
    "_slot": 100
  }
}
```

---
description: Index document with _slot
---

```json
GET /test/define-slot/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.foo._prefix).to.equal('foo.');
  pm.expect(jsonData._schema.schema.foo._index).to.equal('field_values');
  pm.expect(jsonData._schema.schema.foo._ngram).to.equal(false);
  pm.expect(jsonData._schema.schema.foo._cjk_ngram).to.equal(false);
  pm.expect(jsonData._schema.schema.foo._cjk_words).to.equal(false);
  pm.expect(jsonData._schema.schema.foo._slot).to.equal(100);
  pm.expect(jsonData._schema.schema.foo._type).to.equal('string');
});
```

---
description: Get schema with _slot
---

```json
INFO /test/define-slot/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.terms).to.have.property('QKdoc');
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '100']);
   pm.expect(jsonData.values['100']).to.equal('foo and bar');
});
```
---
description: Info document with _slot
---
{% endcomment %}