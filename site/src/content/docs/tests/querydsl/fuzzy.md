---
title: "Fuzzy search"
---

## Search keywords and text with partial

#### Index strin values

```rest
PUT /test/querydsl/fuzzy/keyword/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "keyword",
        "_value": "uncertain"
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

#### Search fuzzy

```rest
SEARCH /test/querydsl/fuzzy/keyword/

```

<!-- e2e:begin
---
params: query=field:unserten%7E3
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Values are valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits[0]['_id']).to.equal('doc');
});
```
e2e:end -->

#### Index text values

```rest
PUT /test/querydsl/fuzzy/text/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "text",
        "_value": "uncertain"
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

#### Search fuzzy

```rest
SEARCH /test/querydsl/fuzzy/text/

```

<!-- e2e:begin
---
params: query=field:unserten%7E3
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Values are valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits[0]['_id']).to.equal('doc');
});
```
e2e:end -->
