---
title: Date Datatype
short_title: Date / Time
---

JSON doesn't have a date datatype, so dates in Xapiand can either be:

* Strings containing formatted dates, e.g. `"2015-11-15T13:12:00"`,
  `"2015-11-15"`, `"2015/11/15 13:12:00"` or ISO-8601.
* A number representing milliseconds-since-the-epoch.
* An object containing a `_date` type.

Internally, dates are converted to UTC (if the time-zone is specified) and
stored as a number representing milliseconds-since-the-epoch.

Queries on dates are internally converted to range queries on this
representation, and the result of aggregations and stored fields is converted
back to a string depending on the date format that is associated with the field.

{% capture req %}

```json
PUT /bank/1?pretty

{
  "MyDate": "2015-11-15T13:12:00"
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
PUT /bank/1?pretty

{
  "MyDate": {
    "_date": {
      "_year": 2015,
      "_month": 11,
      "_day": 15,
      "_time": "13:30:25.123"
    },
    "_accuracy": [ "day", "month", "year" ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Optimized Range Searches

Data types like date and numeric are often used for range search. Due to the way
range searches are implemented, searchig for values is not as performant as
seaching by terms. To improve the performance of the search, Xapiand uses the
`_accuracy` keyword, which indexes terms for value thresholds which later are
used during the querying to improve the filtering and searching.

In the above example terms for the day, month and year are generated.


## Date Math Former

Date math form allow compute math operations in the date before the indexation. The expression starts with an anchor date, which can either be now, or a date string ending with \|\|. This anchor date can optionally be followed by one or more maths expressions:

* +1h - add one hour
* -1d - subtract one day
* /d - round down to the nearest day

The supported units are:

|-----|---------|
| `y` | Years   |
| `M` | Months  |
| `w` | Weeks   |
| `d` | Days    |
| `h` | Hours   |
| `m` | Minutes |
| `s` | Seconds |

For example:
{% capture req %}

```json
PUT /bank/1?pretty

{
    "MyDate": {
      "_value": "2011-01-01||+1y+3M",
      "_type": "date"
    }
}
```
{% endcapture %}
{% include curl.html req=req %}

The adove example is indexed as "`2012-04-01`".


## Parameters

The following parameters are accepted by _Date_ fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_accuracy`                           | Array with the accuracies to be indexed: `"second"`, `"minute"`, `"day"`, `"hour"`, `"month"`, `"year"`, `"decade"`, `"century"`, `"millennium"` |
| `_value`                              | The value for the field (only used at index time)                                       |
| `_index`                              | The mode the field will be indexed as (defaults to `"field_all"`): `"none"`, `"field_terms"`, `"field_values"`, `"field_all"`, `"field"`, `"global_terms"`, `"global_values"`, `"global_all"`, `"global"`, `"terms"`, `"values"`, `"all"`      |
| `_slot`                               | The slot number (it's calculated by default)                                            |
| `_prefix`                             | The prefix the term is going to be indexed with (it's calculated by default)            |
| `_weight`                             | The weight the term is going to be indexed with                                         |


# Time Datatype

The `time` can also be a type without the entire date:

For example:
{% capture req %}

```json
PUT /bank/1?pretty

{
  "MyTime": {
    "_value": "10:12:12.123",
    "_type": "time"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}



# Time Delta Datatype

For example:
{% capture req %}

```json
PUT /bank/1?pretty

{
  "fecha": {
    "_type": "timedelta",
    "_value": "+10:12:12.123"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
