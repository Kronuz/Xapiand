---
title: "Define Slot"
---

## Index document with _slot

#### Index document with _slot

```rest
PUT /test/define-slot/doc

{
  "foo": {
    "_type": "text",
    "_value": "foo and bar",
    "_index": "field_values",
    "_slot": 100
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Get schema with _slot

```rest
GET /test/define-slot/
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.foo._prefix).to.equal('foo.');
  pm.expect(jsonData._schema.foo._index).to.equal('field_values');
  pm.expect(jsonData._schema.foo._ngram).to.equal(false);
  pm.expect(jsonData._schema.foo._cjk_ngram).to.equal(false);
  pm.expect(jsonData._schema.foo._cjk_words).to.equal(false);
  pm.expect(jsonData._schema.foo._slot).to.equal(100);
  pm.expect(jsonData._schema.foo._type).to.equal('text');
});
```
e2e:end -->

#### Info document with _slot

```rest
INFO /test/define-slot/doc
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.values).to.have.all.keys(['0', '1', '100']);
   pm.expect(jsonData.values['100']).to.equal('foo and bar');
});
```
e2e:end -->

#### Search document with _slot by its id

```rest
SEARCH /test/define-slot/

{
  "_query": {
    "_id": "doc"
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("The document is found by its id", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->

#### Search document with _slot by a range of its value

```rest
SEARCH /test/define-slot/

{
  "_query": {
    "foo": {
      "_in": {
        "_range": {
          "_from": "foo and bar",
          "_to": "foo and bar"
        }
      }
    }
  }
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("The value in the slot is found by range", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
});
```
e2e:end -->
