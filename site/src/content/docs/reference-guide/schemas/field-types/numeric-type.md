---
title: "Numeric Datatype"
---

The following _Numeric_ types are supported:

|---------------------------------------|-------------------------------------------------------------------------------|
| `_integer`                            | A 64 bit signed integer type                                                  |
| `_positive`                           | A 64 bit unsigned integer type                                                |
| `_float`                              | A double-precision 64-bit IEEE 754 floating point number, restricted to finite values |

## Exact Values

A numeric field takes only values of its type, and so does a numeric `_id`: in a write, in a search term, in a range
bound and in `q=`. A number qualifies when it is exactly a value of the type: `12` and `12.0` are integers, `1.5` is not,
`-1` is not a `_positive`, and 2^63 is out of range for an `_integer`. Text that spells such a number is the number
(`"12"`, not `" 12"`). An integral float of 2^53 or more is no integer, since a fraction can round to it
(`9007199254740993.5` reads as `9007199254740994.0`); text spelling such an integer exactly (`"9007199254740993"`) is. Anything
else answers `400`, with a message naming the field, the value and the type; nothing is truncated, wrapped, or searched as
something else.

```rest
PUT /exactvalues/1

{
  "count": {
    "_type": "integer",
    "_value": 12
  }
}
```

<!-- e2e:begin
---
description: Exact values: an integer field
---

```js
pm.test("An integer stores", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /exactvalues/2

{
  "count": "13"
}
```

<!-- e2e:begin
---
description: Exact values: text spelling an integer
---

```js
pm.test("Text spelling an integer stores", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /exactvalues/3

{
  "count": 1.5
}
```

<!-- e2e:begin
---
description: Exact values: a fraction in an integer field
---

```js
pm.test("1.5 is no integer", function() {
  pm.response.to.have.status(400);
  var message = pm.response.json().message;
  pm.expect(message).to.include("count");
  pm.expect(message).to.include("1.5");
  pm.expect(message).to.include("integer");
});
```
e2e:end -->

The IDs of this index are integers, set by its first document, so an ID is held to the same rule. Here a restore
names each document's `_id` in its body; the second one is refused and the others are written:

```rest
RESTORE /exactvalues/

[
  { "_id": 4, "count": 4 },
  { "_id": 1.5, "count": 1 },
  { "_id": "5", "count": 5 }
]
```

<!-- e2e:begin
---
description: Exact values: a fraction as an integer ID
---

```js
pm.test("1.5 is no integer ID", function() {
  pm.response.to.be.success;
  var items = pm.response.json().items;
  pm.expect(items.length).to.equal(3);
  pm.expect(items[1].status).to.equal(400);
  pm.expect(items[1].message).to.include("_id");
  pm.expect(items[1].message).to.include("1.5");
  pm.expect(items[1].message).to.include("integer");
  pm.expect(pm.response.json().indexed).to.equal(2);
});
```
e2e:end -->

A search holds its values to the field's type the same way. An exact value matches:

```rest
SEARCH /exactvalues/

{
  "_query": {
    "count": "13"
  }
}
```

<!-- e2e:begin
---
description: Exact values: a search by text spelling the integer
---

```js
pm.test("Text spelling an integer matches", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
  pm.expect(jsonData.hits[0]._id).to.equal(2);
});
```
e2e:end -->

A value of another type answers `400`, in a term, a range bound and `q=`:

```rest
SEARCH /exactvalues/

{
  "_query": {
    "_id": "abc"
  }
}
```

<!-- e2e:begin
---
description: Exact values: a word as an integer ID in a search
---

```js
pm.test("abc is no integer ID", function() {
  pm.response.to.have.status(400);
  var message = pm.response.json().message;
  pm.expect(message).to.include("_id");
  pm.expect(message).to.include("abc");
  pm.expect(message).to.include("integer");
});
```
e2e:end -->

```rest
SEARCH /exactvalues/

{
  "_query": {
    "count": {
      "_in": {
        "_range": {
          "_from": 1.5,
          "_to": 20
        }
      }
    }
  }
}
```

<!-- e2e:begin
---
description: Exact values: a fraction as a range bound
---

```js
pm.test("A range bound of 1.5 is no integer", function() {
  pm.response.to.have.status(400);
  pm.expect(pm.response.json().message).to.include("count");
});
```
e2e:end -->

```rest
SEARCH /exactvalues/?q=count:1.5
```

<!-- e2e:begin
---
description: Exact values: a fraction in q=
---

```js
pm.test("count:1.5 is no integer", function() {
  pm.response.to.have.status(400);
  pm.expect(pm.response.json().message).to.include("count");
});
```
e2e:end -->

To convert a value on purpose, cast it (see [Casting Values](/Xapiand/reference-guide/search/query-dsl/leaf-queries/casting-values)):
`{"_integer": 12.7}` is `12`. A cast converts types; it does not wrap, so `{"_positive": -1}` is still a `400`.

```rest
SEARCH /exactvalues/

{
  "_query": {
    "count": {
      "_integer": 12.7
    }
  }
}
```

<!-- e2e:begin
---
description: Exact values: a cast converts on request
---

```js
pm.test("A cast of 12.7 to integer matches 12", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
  pm.expect(jsonData.hits[0]._id).to.equal(1);
});
```
e2e:end -->

Sources: `Serialize::exact` in `src/serialize.cc`, called by the writes (`src/database/schema.cc`), the searches
(`src/query_dsl.cc`) and the numeric IDs (`Serialize::id`); the casts, `src/cast.cc`.

## Accuracy

Xapiand handles numerical ranges by **trie indexing** numerical values in a
special string-encoded format with variable precision.

All numerical (and also dates, times and geospatial) values are converted to
lexicographic sortable string representations and indexed with different
precisions. A range of values is divided recursively into multiple intervals
for searching: The center of the range is searched only with the lowest possible
precision in the **trie**, while the boundaries are matched more exactly.

Default accuracy in numeric fields is:

```json
[ 100, 1000, 10000, 100000, 1000000, 100000000 ]
```

## Parameters

The following parameters are accepted by _Numeric_ fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_accuracy`                           | Array with the accuracies to be indexed. (Array of numeric values).                     |
| `_value`                              | The value for the field. (Only used at index time).                                     |
| `_index`                              | The mode the field will be indexed as: `"none"`, `"field_terms"`, `"field_values"`, `"field_all"`, `"field"`, `"global_terms"`, `"global_values"`, `"global_all"`, `"global"`, `"terms"`, `"values"`, `"all"`. (The default is `"field_all"`). |
| `_slot`                               | The slot number. (It's calculated by default).                                          |
| `_prefix`                             | The prefix the term is going to be indexed with. (It's calculated by default)           |
| `_weight`                             | The weight the term is going to be indexed with.                                        |
