---
title: Numeric Datatype
short_title: Numeric
---

The following _Numeric_ types are supported:

|---------------------------------------|-------------------------------------------------------------------------------|
| `_integer`                            | A 64 bit signed integer type                                                  |
| `_positive`                           | A 64 bit unsigned integer type                                                |
| `_float`                              | A double-precision 64-bit IEEE 754 floating point number, restricted to finite values |


## Parameters

The following parameters are accepted by _Numeric_ fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_accuracy`                           | Array with the accuracies to be indexed. (Array of numeric values).                     |
| `_value`                              | The value for the field. (Only used at index time).                                     |
| `_index`                              | The mode the field will be indexed as: `"none"`, `"field_terms"`, `"field_values"`, `"field_all"`, `"field"`, `"global_terms"`, `"global_values"`, `"global_all"`, `"global"`, `"terms"`, `"values"`, `"all"`. (The default is `"field_all"`). |
| `_slot`                               | The slot number. (It's calculated by default).                                          |
| `_prefix`                             | The prefix the term is going to be indexed with. (It's calculated by default)           |
| `_weight`                             | The weight the term is going to be indexed with.                                        |
