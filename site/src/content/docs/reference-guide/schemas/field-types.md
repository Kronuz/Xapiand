---
title: "Field Types"
---

Xapiand supports a small number of different datatypes for the fields
in a document:

## Concrete datatypes

### String datatypes

* [Text](../field-types/text-type) (`text`, also spelled `string`)
* [Keyword](../field-types/keyword-type) (`keyword`, also spelled `term`)

### Numeric datatypes

* [Integer](../field-types/numeric-type) (`integer`)
* [Positive](../field-types/numeric-type) (`positive`)
* [Float](../field-types/numeric-type) (`floating`, also spelled `float`)

### Date/Time datatypes

* [Date](../field-types/date-type) (`date`)
* [Datetime](../field-types/date-type) (`datetime`)
* [Time](../field-types/date-type/#time-datatype) (`time`)
* [Timedelta](../field-types/date-type/#time-delta-datatype) (`timedelta`)

### Boolean datatype

* [Boolean](../field-types/boolean-type)

## Geospatial datatypes

* [Geospatial](../field-types/geospatial-type) (`geospatial`, also spelled `geo`)

## UUID datatype

* [UUID](../field-types/uuid-type)

### Script datatype

* `script`: the type of a stored [script](/Xapiand/reference-guide/documents/scripting).

## Complex datatypes

### Object type
* `object`

### Array type
* `array`: written in front of another type, as in `array/integer`.

### Foreign type
* `foreign`: written in front of another type, as in `foreign/object`; the
  [foreign schema](/Xapiand/reference-guide/schemas/foreign-schemas) is the usual use.

Sources: the type names and aliases, `src/database/schema.cc:880-960`.
