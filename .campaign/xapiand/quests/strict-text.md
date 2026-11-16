# strict-text: Numeric text and _term aggregations read exactly

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
is in `state.json`: `campaign status strict-text`.

## Outcome

The two non-blocking findings of the strict-values correctness review close D92's last gaps:
numeric text reads as strictly as JSON numbers, and the `_term` filter aggregation reads values
through `Serialize::exact` like searches do.

## Acceptance criteria

- A1 Numeric text on an integer, positive or float field refuses hex (`"0x10"`) and a leading `+`
  (`"+12"`) with a 400 naming the field, value and type, as JSON input does (review C5); a test
  that fails on master proves it, with a `--fails-without` job.
- A2 The `_term` filter aggregation accepts `12.0` on an integer field and refuses `12.5`, exactly
  as a search does (review C6, R1); a test that fails on master, with a `--fails-without` job.
- A3 The suite passes (the build-test check), and the e2e strict cases from strict-values still
  pass.

## Decisions

- Lead (D92): strict everywhere with a 400; explicit casts are the only conversion.

## May land early

None.
