---
title: "Exists Document API"
---

The _Exists Document API_ allows to check for the existence of a document using
`HEAD` method.

For example:

```rest
HEAD /twitter/tweet/1
```

<!-- e2e:begin
---
description: Document Exists: an existing document
---

```js
pm.test("HEAD of an existing document answers 200", function() {
  pm.response.to.have.status(200);
});
```
e2e:end -->

The result of the above operation is a `200 OK` HTTP response code with no body,
or `404 Not Found` when the document is not there.

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::
