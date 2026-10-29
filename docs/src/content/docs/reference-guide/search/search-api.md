---
title: "Search API"
---

You can search for documents by using the `SEARCH` method:

```rest
SEARCH /bank/
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Search API results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.hits.length).to.equal(10);
});
```
e2e:end -->

:::hint{.tip}
It is also possible to use [HTTP Method Mappings](/Xapiand/reference-guide/api#http-method-mapping).
:::

## Collapsing results

`collapse` keeps the best hit for each distinct value of a field, and drops the rest. Documents
without a value for the field are never collapsed. Here the account holders are collapsed by eye color,
so one account of each color comes back:

```rest
SEARCH /bank/?collapse=eyeColor
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("One hit per eye color", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(3);
  pm.expect(jsonData.hits.length).to.equal(3);
  var colors = jsonData.hits.map(function(hit) { return hit.eyeColor; });
  pm.expect(colors).to.have.members(['blue', 'brown', 'green']);
});
```
e2e:end -->

The hits keep their order (by relevance, or by `sort`), `total` counts the hits that are left, and
`limit` and `offset` apply to them. `collapse_max` keeps more than one hit per value (it is 1 by default).
