# restore-mixed-sign-integers: A msgpack RESTORE keeps every document of a field holding both signs

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status restore-mixed-sign-integers`.

## Outcome

A dump restored through msgpack RESTORE (HTTP or CLI `--restore`) keeps every document, whatever the signs of an integer field\'s values. Today master answers 500 "Negative value of NEGATIVE_INTEGER is not a valid POSITIVE_INTEGER" for half of a 3,000-document corpus (x7-coverage probe 261111-162310-eb4bbc: ndjson RESTORE 3000, dump 3000 entries, msgpack RESTORE indexed 1498).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 A test restores a msgpack stream whose integer field takes positive values first and negative values later, and every document is indexed and served with its value; it fails without the fix: a job (`--fails-without`)
- A2 x7-coverage's corpus (`tests/corpora/source.ndjson`, 3000 documents) round-trips: ndjson RESTORE, `--dump`, msgpack RESTORE give 3000, compared document by document: a job
- A3 [artifact] The cause and the fix, with the probe's numbers: `docs/dev/restore-integers.md`

## Contracts

None (the dump format is unchanged).

## Decisions

- Lead, 11-11: found by x7-gate\'s restore corpora (x7-coverage). The error is msgpack\'s `type_error` (msgpack.h:1483), so a schema or conversion path reads a negative value as unsigned; find where before choosing the fix.

## May land early

The test (A1) and the note.
