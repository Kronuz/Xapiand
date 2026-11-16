---
title: "Stop Words"
---

Xapiand supports a stop word list, which allows you to specify what words
should be removed from a query before processing. This list can be overridden
or stop words can still be searched for if desired, but by default any words
in the active stop words list will not be searched for. A field has a stop word list
only when it has a `_language` (see [text fields](/Xapiand/reference-guide/schemas/field-types/text-type)):
with no language there is no list and no word is dropped. A language with no list
drops nothing and is not an error.

### Example

```rest
SEARCH /bank/

{
  "_query": {
    "personality": "these days are few and far between"
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Stop words are filtered from the query", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.total).to.equal(333);
});
```
e2e:end -->

The stop words in the phrase (`these`, `are`, `few`, `and`, `far`, `between`: all
are in the English list) are removed before the query runs, and only the remaining
content word, `days`, is searched. That is why documents match even though none of them
contain the whole phrase. The words of a group that are stop words are dropped
unless the group holds only stop words.

## Searching of Stop Words

Stop words can be searched by using [Love and Hate Modifiers](/Xapiand/reference-guide/search/query-dsl/leaf-queries/text-queries/love-and-hate)
(by adding `+` to the desired stop word):

```rest
SEARCH /bank/

{
  "_query": {
    "personality": "+these days +are +few +and +far +between"
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Searching of Stop Words results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(0);
  pm.expect(jsonData.hits.length).to.equal(0);
});
```
e2e:end -->

An empty set of stop words under a `_stopwords` key, which would do the same, is not
implemented: `_stopwords` is not a keyword and the request answers `400`
("Invalid operator").

:::hint[Unimplemented Feature!]{.unimplemented}
This feature hasn't yet been implemented...
<br>[Pull requests are welcome!](https://github.com/Kronuz/Xapiand/pulls)
:::

```rest
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_value": "these days are few and far between",
      "_stopwords": []
    }
  }
}
```

<!-- e2e:begin
---
status: 400
---
e2e:end -->
