---
title: "Stem Language"
---

## Index and query stem alike

#### Index an Italian text with an elision

```rest
PUT /test/stemlang/italian/doc

{
  "field": {
    "_type": "text",
    "_language": "italian",
    "_value": "all'art"
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

#### Search the Italian text with the same elision

```rest
SEARCH /test/stemlang/italian/
```

<!-- e2e:begin
---
params: query=field:%22all%27art%22
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
