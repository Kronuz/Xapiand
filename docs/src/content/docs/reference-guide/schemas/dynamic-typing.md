---
title: "Dynamic Typing"
---

One of the most important features of Xapiand is that it tries to get out of
your way and let you start exploring your data as quickly as possible. To index
a document, you don't have to first create an index, define a schema, and
define your fields; you can just index a document and the index, schema, and
fields will spring to life automatically:

```rest
PUT /my_index/1

{
  "name": "John Doe",
  "age": 20
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```rest
GET /my_index/
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Field 'name' is text", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.name._type).to.equal("text");
});
```

```js
pm.test("Field 'age' is integer", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData._schema.age._type).to.equal("integer");
});
```
e2e:end -->

Creates the `/my_index/` index, a schema with two fields: a field called `name`
with datatype `text` and a field called `age` with datatype `integer`.

This automatic detection and addition of new fields is called dynamic typing.

# Datatype Detection

By default, when a previously unseen field is found in a document, Xapiand will
add the new field to the schema. This behaviour can be disabled globally by
running the server in [strict mode](/Xapiand/options#strict), or for one index with
`"_strict": true` in its schema: a field that has no type in a strict schema answers
`412 Precondition Failed`.

Assuming dynamic typing is enabled, some simple rules are used to determine
which datatype the field should have:

| JSON datatype               | Xapiand datatype            |
|-----------------------------|-----------------------------|
| null                        | No type                     |
| Boolean (`true` or `false`) | `boolean`                   |
| Floating point number       | `floating`                  |
| Integer                     | `integer`                   |
| Object                      | `object`                    |
| Array                       | `array`, or `geospatial` if it is two or three numbers in longitude and latitude range |
| String                      | See [String detection](#string-detection) |

A MessagePack body gives an unsigned integer the type `positive`. The schema shows the geospatial type as `geo`.

## String detection

A string is tested against these patterns, in order, and takes the type of the first
that matches:

1. `uuid`: a UUID (`_uuid_detection`).
2. `date`: a date such as `2015-09-02` (`_date_detection`).
3. `datetime`: a date and time such as `2015-09-02T10:00:00` or `2015/09/02` (`_datetime_detection`).
4. `time`: a time of day such as `12:30:00` (`_time_detection`).
5. `timedelta`: a signed or fractional span of time (`_timedelta_detection`).
6. `geospatial`: an EWKT shape such as `POINT(1 2)` (`_geo_detection`).
7. `boolean`: the strings `true` and `false` (`_bool_detection`).
8. `text`: any other string (`_text_detection`).
9. `keyword`: any other string, when `_text_detection` is `false`.

A string that looks like a number is a `text` (or `keyword`): numeric detection reads
numbers, never strings.

Every `_..._detection` flag defaults to `true`, and is set in the index's schema like any other
property (the name starts with an underscore, as every reserved word does):

```rest
PUT /dyn_typing/

{
  "_schema": {
    "_text_detection": true,
    "_numeric_detection": true
  }
}
```

```rest
PUT /dyn_typing/1

{
  "age": 20,
  "balance": -5,
  "ratio": 1.5,
  "flag": "true",
  "created": "2015-09-02",
  "updated": "2015-09-02T10:00:00",
  "opened": "12:30:00",
  "id": "a8098c1a-f86e-11da-bd1a-00112444be1e",
  "place": "POINT(1 2)",
  "position": [-74.01, 41.5],
  "title": "hello world",
  "number_text": "1.5"
}
```

```rest
GET /dyn_typing/
```

<!-- e2e:begin
---
description: Dynamic typing: the detected types
---

```js
pm.test("Each value gets the type its pattern detects", function () {
  pm.response.to.be.success;
  var schema = pm.response.json()._schema;
  pm.expect(schema.age._type).to.equal("integer");
  pm.expect(schema.balance._type).to.equal("integer");
  pm.expect(schema.ratio._type).to.equal("floating");
  pm.expect(schema.flag._type).to.equal("boolean");
  pm.expect(schema.created._type).to.equal("date");
  pm.expect(schema.updated._type).to.equal("datetime");
  pm.expect(schema.opened._type).to.equal("time");
  pm.expect(schema.id._type).to.equal("uuid");
  pm.expect(schema.place._type).to.equal("geo");
  pm.expect(schema.position._type).to.equal("geo");
  pm.expect(schema.title._type).to.equal("text");
  pm.expect(schema.number_text._type).to.equal("text");
});
```
e2e:end -->

These are the only field datatypes that are dynamically detected. Any other
datatypes must be mapped explicitly.

Sources: `src/database/schema.cc:5090-5260` (`guess_concrete_type`: the order),
`:1885-1910` (the defaults), `src/reserved/schema.h:65-74` (the names).

## Disabling datetime detection

Dynamic datetime detection can be disabled by setting `_datetime_detection` to
`false` in the index schema:

```rest
PUT /dyn_datetime/

{
  "_schema": {
    "_datetime_detection": false
  }
}
```

Then, a new field that looks like a datetime is not detected as `datetime`, and
ends up being `text`. (A plain `YYYY-MM-DD` date is still a `date`: that has its own flag,
`_date_detection`.)

```rest
PUT /dyn_datetime/1

{
  "create": "2015/09/02",
  "day": "2015-09-02"
}
```

```rest
GET /dyn_datetime/
```

<!-- e2e:begin
---
description: Dynamic typing: datetime detection off
---

```js
pm.test("The datetime-looking string is text", function () {
  pm.response.to.be.success;
  var schema = pm.response.json()._schema;
  pm.expect(schema.create._type).to.equal("text");
  pm.expect(schema.day._type).to.equal("date");
});
```
e2e:end -->

## Numeric detection

`_numeric_detection` (on by default) decides whether JSON numbers are detected as `integer`
or `floating`. It never reads strings: a string such as `"1.0"` is a `text` whatever the
setting. While JSON has support for native floating point and integer datatypes, some
applications or languages may sometimes render numbers as strings; the correct solution
is then to map these fields explicitly, with [explicit types](../explicit-types) or
[casting](../casting-types).

## Text detection

`_text_detection` (on by default) types a string that matched no other pattern as `text`.
With `"_text_detection": false` such a string is a `keyword`.

## UUID detection

`_uuid_detection` (on by default) types a string in UUID format as `uuid`. Field
*names* that are UUIDs are a different matter: see
[Dynamic Field Names](../dynamic-field-names).
