---
title: Options
---

## Verbosity

Verbosity of the servers logs can be set by using the `-v`, `--verbose` or
`--verbosity` options. _*very-very-very* verbose_ output is usually enabled
with `-vvvv` or `--verbosity=4`. This mode also enables `--echo`, `--pretty` and
`--comments` options by default.


### Echo

Echo makes Xapiand return newly created or edited objects as part of the
response. Usually when creating a new object Xapiand will return `201 Created`
HTTP response code, without a body and `204 No Content` HTTP response when
updating existing objects, also without a body. Returning a body can be enabled
with the `--echo` option or by using a verbosity level higher or equal to 4.


### Pretty

Pretty makes Xapiand return pretty (formatted) JSON output as responses. This
option can be enabled with the `--pretty` option or by using a verbosity level
higher or equal to 4.


### Strict

Schemas are normally automatically created by default, guessin gthe type of new
fields being indexed. The `--strict` option disables this dynamic fields feature
and forces the user to specify a type for all new fields, making the request
fail with `412 Precondition Failed` if the datatype is missing.


{: .note .construction }
_This section is a **work in progress**..._
