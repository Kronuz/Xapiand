---
title: "Basque Language"
---

## Index and search

#### Index a Basque text

```rest
PUT /test/stemlang/basque/doc

{
  "field": {
    "_type": "text",
    "_language": "basque",
    "_value": "etxeak"
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

#### Search the Basque text

```rest
SEARCH /test/stemlang/basque/
```

<!-- e2e:begin
---
params: query=field:etxeak
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("The indexed document matches", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(1);
  pm.expect(jsonData.hits[0]['_id']).to.equal('doc');
});
```
e2e:end -->
