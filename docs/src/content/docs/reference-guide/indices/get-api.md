---
title: "Get Index API"
---

The _Get Index API_ allows to retrieve information about one or more indexes.

```rest
GET /twitter/tweet/
```

<!-- e2e:begin
```js
pm.test("returns the index schema and settings", function () {
  var body = pm.response.json();
  pm.expect(body).to.have.property("_schema");
  pm.expect(body).to.have.property("_settings");
});
```
e2e:end -->

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
