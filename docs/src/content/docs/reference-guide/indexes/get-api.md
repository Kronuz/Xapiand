---
title: "Get Index API"
---

The _Get Index API_ allows to retrieve the settings and the schema of an index. It takes a single index: a list answers `400`.

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

```rest
GET /twitter/tweet,twitter/user/
```

<!-- e2e:begin
---
description: Get index: a list of indexes
---

```js
pm.test("A list of indexes is refused", function () {
  pm.response.to.have.status(400);
});
```
e2e:end -->

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
