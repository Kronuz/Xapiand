---
title: "Exists Index API"
---

The _Exists Index API_ is used to check if an index exists or not. It takes a single index: a list answers `400`.

For example:

```rest
HEAD /twitter/tweet/
```

<!-- e2e:begin
---
description: Index Exists: an existing index
---

```js
pm.test("HEAD of an existing index answers 200", function() {
  pm.response.to.have.status(200);
});
```
e2e:end -->

The answer is `200 OK` when the index exists and `404 Not Found` when it does not.

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
