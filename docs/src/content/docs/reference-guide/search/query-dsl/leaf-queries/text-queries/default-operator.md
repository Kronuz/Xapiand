---
title: "Default Operator"
---

The default operator for text queries can be set by using the desired modifier.

## OR

_OR_ uses the `_or` modifier (the default).

## AND

_AND_ uses the `_and` modifier.

### Example

To make **AND** the default operator and thus forcing a query to search for
**all** terms instead of _any_ term (which is the default):

```rest
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_and": "these days are few and far between"
    }
  }
}
```

<!-- e2e:begin
---
params: sort=_id
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Default operator count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("Default operator size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("Default operator values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [2, 4, 8, 9, 11, 15, 16, 26, 27, 29];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->

## Other modifiers

The default operator of the words of a text can also be:

- `_near`, `_phrase`: the words as a [near](../near) or [phrase](../phrase) group.
- `_synonym`: the words are synonyms, and weigh once.
- `_max`: the weight is the maximum of the words' weights, not the sum.

Choosing `_and` or `_or` also lets the text use the boolean words (`AND`, `OR`, `NOT`, `XOR`,
`NEAR`, `ADJ`) described in the [query string grammar](../../text-queries#the-query-string-grammar);
a plain string reads them as words.
Sources: `src/query_dsl.cc:805-823`.
