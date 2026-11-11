---
title: "Love and Hate Query"
---

The `+` and `-` operators, select documents based on the presence or absence of
specified terms.

:::hint[Note]{.info}
When using these operators, _stop words_ do not apply.
:::

A lone exclusion, such as `-ambitious` with nothing required, is not a query: under `_and`
or `_or` on a field with no stop word list it answers `400` ("Syntax: <expression> NOT
<expression>").

#### Example

The following matches all documents with the phrase _"adventurous nature"_ but
not _ambitious_; and:

```rest
SEARCH /bank/

{
  "_query": {
    "personality": "\"adventurous nature\" -ambitious"
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
pm.test("Love and Hate count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("Love and Hate size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("Love and Hate values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [24, 37, 268, 340, 378, 380, 400, 448, 479, 492];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->

:::hint[Caution]{.caution}
One thing to note is that the behaviour of the +/- operators vary depending on
the default operator used and the above examples assume that the default (`OR`)
is used.
:::
