---
title: Define Prefix
---

## Index document with _prefix

#### Index document with _prefix

{% capture req %}

```json
PUT /test/define-prefix/doc

{
  "foo": {
    "_type": "text",
    "_value": "foo and bar",
    "_index": "field_terms",
    "_prefix": "bar."
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

####  Get schema with _prefix

{% capture req %}

```json
GET /test/define-prefix/
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
  pm.expect(jsonData._schema.foo._prefix).to.equal('bar.');
  pm.expect(jsonData._schema.foo._index).to.equal('field_terms');
  pm.expect(jsonData._schema.foo._type).to.equal('text');
  pm.expect(jsonData._schema.foo._ngram).to.equal(false);
  pm.expect(jsonData._schema.foo._cjk_ngram).to.equal(false);
  pm.expect(jsonData._schema.foo._cjk_words).to.equal(false);
});
```
{% endcomment %}

#### Info document with _prefix

{% capture req %}

```json
INFO /test/define-prefix/doc
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
  pm.expect(jsonData.terms.bar).to.have.all.keys(['Sand', 'Sbar', 'Sfoo']);
  pm.expect(jsonData.values).to.have.all.keys(['0', '1']);
});
```
{% endcomment %}