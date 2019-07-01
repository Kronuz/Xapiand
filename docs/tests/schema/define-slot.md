---
title: Define Slot
---

## Index document with _slot

#### Index document with _slot

{% capture req %}

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
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
{% endcomment %}

#### Get schema with _slot

{% capture req %}

```json
GET /test/define-slot/
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
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
{% endcomment %}

#### Info document with _slot

{% capture req %}

```json
INFO /test/define-slot/doc
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
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
{% endcomment %}