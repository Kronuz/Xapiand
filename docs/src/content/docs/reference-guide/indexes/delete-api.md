---
title: "Delete Index API"
---

:::hint[Unimplemented Feature!]{.unimplemented}
This feature hasn't yet been implemented...
<br>[Pull requests are welcome!](https://github.com/Kronuz/Xapiand/pulls)
:::

The _Delete Index API_ allows to delete an existing index.

```rest
DELETE /twitter/tweet/
```

<!-- e2e:begin
---
status: 501
---
e2e:end -->

:::hint{.warning}
`DELETE /twitter/tweet/` is not the same as `DELETE /twitter/tweet`, the former will delete the
whole index `/twitter/tweet/` while the later will delete document `twitter/tweet` inside
index `/`.
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
