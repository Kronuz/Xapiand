---
title: Compound Query Clauses
---

Compound query clauses wrap other leaf or compound queries and are used to
combine multiple queries in a logical fashion (such as boolean query), or to
alter their behaviour.

* [**_Logical Operators_**](logical-operators)
* [**_Maybe Operator_**](maybe-operator)
* [**_Filter Operator_**](filter-operator)
* [**_Elite Set Operator_**](elite-set-operator)
* [**_Range Searches_**](range-searches)
* [**_Geospatial Searches_**](geospatial-searches)


<!--
* `_max`          - Pick the maximum weight of any subquery. This matches the
                    same documents as a `_or`, but the weight contributed is
                    the maximum weight from any matching subquery (for `_or`,
                    it's the sum of the weights from the matching subqueries).
* `_wildcard`     - Wildcard expansion.
* `_scale_weight` -
* `_synonym`      -
 -->
