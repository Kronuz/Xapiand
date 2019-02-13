---
title: Numeric Datatype
---

The following numeric types are supported:

|---------------------------------------|-------------------------------------------------------------------------------|
| `_integer`                            | A signed integer type with width of  64 bits                                  |
| `_positive`                           | A unsigned integer type with width of exactly 64 bits                         |
| `_float`                              | A double-precision 64-bit IEEE 754 floating point number, restricted to finite values                                                                     |

## Parameters for the text fields

The following parameters are accepted by text fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_value`                              | The value for the field                                                                 |
| `_slot`                               | The slot number                                                                         |
| `_index`                              | One or a pair of : `none`, `field_terms`, `field_values`, `field_all`, `field`, `global_terms`, `global_values`, `global_all`, `global`, `terms`, `values`, `all`      |
| `_prefix`                             | The prefix with the term is going to be indexed     |
| `_weight`                             | The weight with the term is going to be indexed     |
| `_accuracy`                           | Array of numeric values                             |