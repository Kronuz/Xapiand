---
title: Date Datatype
short_title: Date
---

JSON doesn't have a date datatype, so dates in Xapiand can either be:

* Strings containing formatted dates, e.g. `"2015-11-15T13:12:00"`, `"2015-11-15"`
or `"2015/11/15 13:12:00"`.
* A number representing milliseconds-since-the-epoch.
* An object containing a `_date` type.

Internally, dates are converted to UTC (if the time-zone is specified) and
stored as a number representing milliseconds-since-the-epoch.

Queries on dates are internally converted to range queries on this
representation, and the result of aggregations and stored fields is converted
back to a string depending on the date format that is associated with the field.

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

## Date Math former

Date math form allow compute math operations in the date before the indexation. The expression starts with an anchor date, which can either be now, or a date string ending with \|\|. This anchor date can optionally be followed by one or more maths expressions:

* +1h - add one hour
* -1d - subtract one day
* /d - round down to the nearest day

The supported units are:

|-----|--------|
| `y` | years  |
| `M` | months |
| `w` | weeks  |
| `d` | days   |
| `h` | hours  |
| `m` | minutes|
| `s` | seconds|

For example:
{% capture req %}

```json
PUT /bank/1:search?pretty

{
    "MyDate": {
      "_value": "2011-01-01||+1y+3M",
      "_type": "date"
    }
}
```
{% endcapture %}
{% include curl.html req=req %}

The adove example is indexed as "2012-04-01".

## Parameters for the text fields

The following parameters are accepted by text fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_index`                              | One or a pair of : `none`, `field_terms`, `field_values`, `field_all`, `field`, `global_terms`, `global_values`, `global_all`, `global`, `terms`, `values`, `all`      |
| `_value`                              | The value for the field                                                                 |
| `_slot`                               | The slot number                                                                         |
| `_prefix`                             | The prefix with the term is going to be indexed     |
| `_weight`                             | The weight with the term is going to be indexed     |
| `_accuracy`                           | `second`, `minute`, `day`, `hour`, `month`, `year`, `decade`, `century`, `millennium`   |