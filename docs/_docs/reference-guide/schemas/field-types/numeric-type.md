---
title: Numeric Datatype
short_title: Numeric
---

The following _Numeric_ types are supported:

|---------------------------------------|-------------------------------------------------------------------------------|
| `_integer`                            | A 64 bit signed integer type                                                  |
| `_positive`                           | A 64 bit unsigned integer type                                                |
| `_float`                              | A double-precision 64-bit IEEE 754 floating point number, restricted to finite values |


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
[
  1000,
  10000,
  100000,
  1000000,
  10000000,
  1000000000,
  100000000000,
  10000000000000,
  1000000000000000,
  100000000000000000,
  1000000000000000000,
  10000000000000000000
]
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
