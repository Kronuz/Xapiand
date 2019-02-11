---
title: Date Datatype
---

JSON doesn’t have a date datatype, so dates in Xapiand can either be:

strings containing formatted dates, e.g. "2015-11-15T13:12:00", "2015-11-15" or "2015/11/15 13:12:00".
a long number representing milliseconds-since-the-epoch.
an integer representing seconds-since-the-epoch.
Internally, dates are converted to UTC (if the time-zone is specified) and stored as a long number representing milliseconds-since-the-epoch.

Queries on dates are internally converted to range queries on this long representation, and the result of aggregations and stored fields is converted back to a string depending on the date format that is associated with the field.

{% capture req %}

```json
PUT /bank/1:search?pretty

{
  "MyDate": "2015-11-15T13:12:00"
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
PUT /bank/1:search?pretty

{
  "MyDate": {
    "_date": {
      "_year": 2015,
      "_month": 11,
      "_day": 15,
      "_time": "13:30:25.123"
    },
    "_accuracy": [ "day", "month", "year"]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

## Optimized Range Searches

Data types like date and numeric are often used for range search. Due to the range searches are performed with [values](https://xapian.org/docs/facets.html) there are not equally fast like term search, to improve the performance of the search we can use the keyword `_accuracy` which index as terms thresholds and are added to the query to improve the filtering and searching for the values. In the above example terms for the day, month and year are generated.


## Parameters for the text fields

The following parameters are accepted by text fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_index`                              | One or a pair of : `none`, `field_terms`, `field_values`, `field_all`, `field`, `global_terms`, `global_values`, `global_all`, `global`, `terms`, `values`, `all`      |
| `_value`                              | The value for the field                                                                 |
| `_slot`                               | The slot number                                                                         |
| `_prefix`                             | The prefix with the term is going to be indexed     |
| `_weight`                             | The weight with the term is going to be indexed     |
| `_accuracy`                           | `second`, `minute`, `day`, `hour`, `month`, `year`, `decade`, `century`, `millennium`   |