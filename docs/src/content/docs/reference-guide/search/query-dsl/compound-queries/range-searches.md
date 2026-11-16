---
title: "Range Searches"
---

The keyword `_range` matches documents where the given value is between the
given `_from` and `_to` fixed range (including both endpoints).

If you only use the keyword `_from` matches documents where the given value is
greater than or equal to a fixed value.

If you only use the keyword `_to` matches documents where the given value is
less than or equal a fixed value.

#### Example

This example find _all_ bank accounts for which their account holders are
_females_ in the ages between 20 and 30:

```rest
SEARCH /bank/

{
  "_query": {
    "_and": [
      { "gender": "female" },
      {
        "age": {
          "_in": {
            "_range": {
              "_from": 20,
              "_to": 30
            }
          }
        }
      }
    ]
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
pm.test("Range Searches count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("Range Searches size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("Range Searches values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [4, 5, 10, 17, 23, 34, 43, 44, 58, 59];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->

## Open ranges and terms-only fields

A `_range` with neither `_from` nor `_to` matches every document that has the field. Ranges
work on numbers, dates, times and (by their text order) strings.

A field indexed with terms only (`_index: field_terms`, see
[Indexing Mode](/Xapiand/reference-guide/schemas/indexing-mode)) has no values to check a range
against, so the range is answered by the field's [accuracy](/Xapiand/reference-guide/schemas/accuracy)
terms: a closed range returns the documents of the buckets it covers (which can be wider than the
range by less than the finest bucket), and a range that is open at one end, or wider than the
terms can cover, answers `400`.
Sources: `src/multivalue/range.cc:362-364,412-428`, `src/query_dsl.cc:453`,
`docs/mechanisms/numeric-range-terms.md`.
