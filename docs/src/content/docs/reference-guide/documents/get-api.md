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
  "_version": 1,
  "#docid": 1,
  "#shard": 1
}
```

The above result includes the `_id` and `_version` of the document we wish to
retrieve, additionally to the actual body of the document.

If the document is not found, it will return a `404 Not Found` HTTP response code.

:::hint{.warning}
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

## Volatile

By passing `volatile` query param to the request, you can ensure the operation
will return the latest committed document from the primary shard.

:::hint{.caution}
Try limiting the use of `volatile` as it will hit performance.
:::
