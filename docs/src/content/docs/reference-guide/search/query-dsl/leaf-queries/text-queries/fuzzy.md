---
title: "Fuzzy Query"
---

The Fuzzy Query generates matching terms that are within the maximum edit
distance (2 by default) and then checks the term dictionary to find out which of
those generated terms actually exist in the index. The query is not capped: it
uses every matching term (the 50-term cap belongs to the [partial](../partial)
query).

### Example

```rest
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": {
      "_fuzzy": "banna"
    }
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

A similar effect could be obtained by using the tilde ("`~`") character
followed optionally by the edit distance as part of the query text:

```rest
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": "banna~"
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

:::hint{.info}
Edit distance defaults to 2, so `"banna~"` and `"banna~2"` are equivalent. The `~` and its
digit are read only at the end of the text: `"banna~2 apple"` does not have a fuzzy word.
:::

## Parameters

:::hint[Unimplemented Feature!]{.unimplemented}
QueryDSL parameters are not implemented yet.
<br>[Pull requests are welcome!](https://github.com/Kronuz/Xapiand/pulls)
:::

The following parameters are planned for the `_fuzzy` query (the only accepted form today is a string):

|--------------------------------------|-------------------------------------------------------|
| `_fuzziness`                         | The maximum edit distance. Defaults to `2`.           |
| `_max_expansions`                    | The maximum number of terms that the fuzzy query will expand to. No limit by default. |
| `_prefix_length`                     | The number of initial characters which will not be "fuzzified". This helps to reduce the number of terms which must be examined. Defaults to `0`. |

:::hint[Warning]{.warning}
This query can be very heavy if `_prefix_length` is set to `0` and if
`_max_expansions` is left unlimited. It could result in every term
in the index being examined!
:::
