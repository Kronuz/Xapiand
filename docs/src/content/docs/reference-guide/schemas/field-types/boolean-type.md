---
title: "Boolean Datatype"
---

Boolean fields accept JSON `true` and `false` values, but can also accept
strings which are interpreted as either `true` or `false`. The strings `"true"` and `"false"`
are detected as booleans by default; the others (`"t"`, `"f"`, `"1"`, `"0"`) need the `_type`
`boolean` to be explicit. The two following examples are equivalent:

```rest
UPDATE /bank/1

{
    "isPublished": true
}
```

<!-- e2e:begin
---
description: Boolean: a detected boolean
---

```js
pm.test("A JSON boolean is stored", function() {
  pm.response.to.be.success;
  var doc = pm.response.json();
  pm.expect(doc._id).to.equal(1);
  pm.expect(doc.isPublished).to.deep.equal(true);
});
```
e2e:end -->

```rest
UPDATE /bank/1

{
    "isPublished": {
      "_value": "true",
      "_type": "boolean"
    }
}
```

<!-- e2e:begin
---
description: Boolean: an explicit boolean
---

```js
pm.test("A typed boolean is stored", function() {
  pm.response.to.be.success;
  var doc = pm.response.json();
  pm.expect(doc._id).to.equal(1);
  pm.expect(doc).to.have.property("isPublished");
});
```
e2e:end -->

When using strings, you can use any of the following and it gets interpreted
as either `true` or `false`:

* `"true"`, `"t"`, `"1"` -> `true`
* `"false"`, `"f"`, `"0"` -> `false`

## Parameters

The following parameters are accepted by _Boolean_ fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_value`                              | The value for the field. (Only used at index time).                                     |
| `_index`                              | The mode the field will be indexed as: `"none"`, `"field_terms"`, `"field_values"`, `"field_all"`, `"field"`, `"global_terms"`, `"global_values"`, `"global_all"`, `"global"`, `"terms"`, `"values"`, `"all"`. (The default is `"field_all"`). |
| `_slot`                               | The slot number. (It's calculated by default).                                          |
| `_prefix`                             | The prefix the term is going to be indexed with. (It's calculated by default)           |
| `_weight`                             | The weight the term is going to be indexed with.                                        |
| `_bool_term`                          | Index the value as a boolean term, with no weight. (The default is `false`). Every term type accepts it. |
