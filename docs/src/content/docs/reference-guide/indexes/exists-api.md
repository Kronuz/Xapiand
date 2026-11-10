---
title: "Exists Index API"
---

The _Exists Index API_ is used to check if an index exists or not. It takes a single index: a list answers `400`.

For example:

```rest
HEAD /twitter/tweet/
```

The answer is `200 OK` when the index exists and `404 Not Found` when it does not.

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
