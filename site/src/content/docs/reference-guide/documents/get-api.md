---
title: "Get Document API"
---

The _Get API_ allows to get a document from the index based on its ID. The
following example gets a document from an index called _"twitter"_, with ID
valued `1`:

```rest
GET /twitter/tweet/1
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Get Document API document", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData["user"]).to.equal("Kronuz");
  pm.expect(jsonData["post_date"]).to.equal("2019-03-22T14:35:26");
  pm.expect(jsonData["message"]).to.equal("Trying out Xapiand");
});
```
e2e:end -->

The result of the above get operation is a `200 OK` HTTP response code with the
following body:

```json
{
  "user": "Kronuz",
  "post_date": "2019-03-22T14:35:26",
  "message": "Trying out Xapiand",
  "_id": 1,
  "_version": 1
}
```

The above result includes the `_id` and `_version` of the document we wish to
retrieve, additionally to the actual body of the document.

If the document is not found, it will return a `404 Not Found` HTTP response code.

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

## Comments

By passing the `comments` query param (or starting the node with `--comments`,
which `-vvvv` implies), the result carries the engine's bookkeeping too: `#docid`
is the document's number in the index and `#shard` the shard that holds it.

```rest
GET /twitter/tweet/1?comments=true
```

<!-- e2e:begin
---
description: Get: comments
---

```js
pm.test("The comments add #docid and #shard", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData["#docid"]).to.be.a("number");
  pm.expect(jsonData["#shard"]).to.be.a("number");
  pm.expect(jsonData["user"]).to.equal("Kronuz");
});
```
e2e:end -->

## Selector

A selector keeps one part of the document. Pass it in the `selector` query param,
or after the ID, as in `GET /twitter/tweet/1.user`:

```rest
GET /twitter/tweet/1?selector=user
```

<!-- e2e:begin
---
description: Get: selector
---

```js
pm.test("The selector keeps the user", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json()).to.equal("Kronuz");
});
```
e2e:end -->

## Volatile

By passing the `volatile` query param to the request, the operation opens the
index for writing, so the read sees the latest view the writer has.

:::hint{.caution}
Try limiting the use of `volatile` as it will hit performance.
:::

Sources: `src/server/search_views.cc:1834-1889` (the get and the `#docid`, `#shard` fields,
only with `request.comments`), `src/opts.cc:252-255` (`--comments`), and
`src/server/search_views.cc:2371-2380` (`volatile`).
