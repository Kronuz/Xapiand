---
title: "Delete Index API"
---

:::hint[Unimplemented Feature!]{.unimplemented}
This feature hasn't yet been implemented: `DELETE` on an index path answers `501 Not Implemented`.
<br>[Pull requests are welcome!](https://github.com/Kronuz/Xapiand/pulls)
:::

The _Delete Index API_ is meant to delete an existing index. Today no request deletes
an index over HTTP.

```rest
DELETE /twitter/tweet/
```

<!-- e2e:begin
---
description: Delete Index: not implemented
---

```js
pm.test("DELETE of an index answers 501 Not Implemented", function() {
  pm.response.to.have.status(501);
});
```
e2e:end -->

:::hint{.warning}
`DELETE /twitter/tweet/` is not the same as `DELETE /twitter/tweet`, the former
names the index `/twitter/tweet/` (and answers `501`) while the later deletes document
`twitter/tweet` inside index `/`. `DELETE /` answers `405`.
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
