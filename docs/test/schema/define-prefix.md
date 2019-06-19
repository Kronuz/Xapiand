---
title: Define Prefix
---

### Index document with _prefix

{% comment %}
```json
PUT /test/define-prefix/doc

{
  "foo": {
    "_type": "string",
    "_value": "foo and bar",
    "_index": "field_terms",
    "_prefix": "bar."
  }
}
```

---
description: Index document with _prefix
---

```json
GET /test/define-prefix/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.schema.foo._prefix).to.equal('bar.');
  pm.expect(jsonData._schema.schema.foo._index).to.equal('field_terms');
  pm.expect(jsonData._schema.schema.foo._type).to.equal('string');
  pm.expect(jsonData._schema.schema.foo._ngram).to.equal(false);
  pm.expect(jsonData._schema.schema.foo._cjk_ngram).to.equal(false);
  pm.expect(jsonData._schema.schema.foo._cjk_words).to.equal(false);
});
```

---
description: Get schema with _prefix
---

```json
INFO /test/define-prefix/doc
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
  pm.expect(jsonData.terms.bar).to.have.all.keys(['Sand', 'Sbar', 'Sfoo']);
  pm.expect(jsonData.values).to.have.all.keys(['0', '1']);
});
```
---
description: Info document with _prefix
---
{% endcomment %}