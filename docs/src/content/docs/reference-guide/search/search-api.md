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
