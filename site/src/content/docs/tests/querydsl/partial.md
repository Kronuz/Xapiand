---
title: "Partial search"
---

## Search keywords and text with partial

#### Index keywords values

```rest
PUT /test/querydsl/partial/keyword/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "keyword",
        "_value": "categorisation"
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

#### Search partial

```rest
SEARCH /test/querydsl/partial/keyword/

```

<!-- e2e:begin
---
params: query=field:cate**
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
PUT /test/querydsl/partial/text/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "text",
        "_value": "dynamic categorisation"
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

#### Search partial

```rest
SEARCH /test/querydsl/partial/text/

```

<!-- e2e:begin
---
params: query=field:dynamic%20cate**
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
