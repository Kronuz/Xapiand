# stemmer-names: Every stemmer name Xapiand's schema accepts stems as the Xapian-era release did, or is removed

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status stemmer-names`.

## Outcome

The six names release-rows found (`earlyenglish`, `german2`, `nb`, `nn` stemmed by Xapian but refused
by snowball; `english_lovins`, `english_porter` refused by both; prospect `docs/gates/release-rows.md`
A4) are served with vocabularies equal to the oracle's, or removed from the schema, docs and tests
(remove-not-refuse). Removing a name narrows user-visible behavior: the owner decides per name first.

## Acceptance criteria

- A1 [decision] The owner's call per name (serve or remove): a PLAN.md Decisions id
- A2 Every name the schema accepts stems equal to the oracle's vectors, word for word: a job (release-rows' stems row rerun at this quest's code)
- A3 A removed name is refused with a 400 naming the accepted languages, and appears nowhere in docs: a test in the build-test check

## Contracts

The schema's language list (user-visible).

## Decisions

- Lead, 11-11: split from release-rows A4 (review accepted at prospect:0ba797e).

## May land early

Nothing before A1.
