# script-method: a script sees the request's method in `_method`

Risk: normal. The lead writes this quest; contributors read it and never edit it.

## Outcome

A document script reads the HTTP method of the write that runs it in `_method` ("PUT", "PATCH",
"POST", ...). Today `DatabaseHandler::call_script` (src/database/handler.cc:456) declares
`std::string method;  // TODO` and passes it empty, so `_method` is always "". The method comes from
the request (`method_token`, src/server/search_views.cc:529) through the write entry points to a
member of `DatabaseHandler` (handler.h). Scripts are reached only from `Schema::index`
(src/database/schema.cc:2912).

## Acceptance criteria

- A1 A script sees "PUT" on a PUT, "PATCH" on a PATCH, "POST" on a POST (and on a bulk or restore
  item, the method its line names, or the request's when it names none): a test with values, plus a
  `--fails-without` job at the handler's empty method
- A2 Paths that index without a request (replication, restore from a dump, internal reindex) pass a
  defined method and are listed in the journal with the value each gets: the test covers one of them
- A3 The suite passes and CI is green: `ci:Build + unit + smoke`, `ci:Functional E2E (docs)`
- A4 [artifact] The script docs name `_method` and its values: `docs/src/content/docs/reference-guide/documents/scripting.md`

## Decisions

- An internal path with no request method passes "" only if the docs say so; prefer the method of
  the write that caused it.

## May land early

Nothing.
