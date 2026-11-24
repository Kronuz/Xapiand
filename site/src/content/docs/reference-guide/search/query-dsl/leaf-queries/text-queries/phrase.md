---
title: "Phrase Query"
---

A phrase is surrounded with double quotes (`""`) and allows searching for a
specific exact phrase and returns only matches where all terms appear in the
document in the correct order, giving a weight of the sum of each term.
For example:

* Documents which match A followed by B followed by C gives a weight of A+B+C

:::hint[Note]{.info}
When searching for phrases, _stop words_ do not apply.
:::

:::hint[Caution]{.caution}
Hyphenated words are also treated as phrases, as are cases such as filenames
and email addresses (e.g. `/etc/passwd` or `president@whitehouse.gov`)
:::

A wildcard, fuzzy word or group inside a phrase is refused with `400`: a phrase is made of
plain words.

#### Example

In the following example, we will retrieve documents with the exact phrase,
including the stop words `these`, `come`, `a` and `of`.

```rest
SEARCH /bank/

{
  "_query": {
    "personality": "\"these come alongside a habit of\""
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
pm.test("Example results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.hits.length).to.equal(10);
});
```
e2e:end -->

```rest
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_phrase": "these come alongside a habit of"
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
pm.test("phrase query count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("phrase query size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("phrase query values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [5, 9, 23, 56, 58, 65, 100, 119, 125, 155];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->
