---
title: "Exists Document API"
---

The _Exists Document API_ allows to check for the existence of a document using
`HEAD` method.

For example:

```rest
HEAD /twitter/tweet/1
```

The result of the above operation is a `200 OK` HTTP response code with no body.

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
