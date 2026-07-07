---
title: "Wildcard search"
---

## Search keywords and text with wilcards

#### Index keywords values

```rest
PUT /test/querydsl/wildcard/keyword/doc

{
    "_id": {
        "_type": "keyword",
    },
    "field": {
        "_type": "keyword",
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

#### Search wildcard

```rest
SEARCH /test/querydsl/wildcard/keyword/

```

<!-- e2e:begin
---
params: query=field:dynamic%20cate*
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

#### Index text values

```rest
PUT /test/querydsl/wildcard/text/doc

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

#### Search wildcard

```rest
SEARCH /test/querydsl/wildcard/text/

```

<!-- e2e:begin
---
params: query=field:dynamic%20cate*
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
