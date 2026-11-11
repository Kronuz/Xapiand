# docs-site-icons: The docs site serves the brand's light and dark variants

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status docs-site-icons`.

## Outcome

The docs site serves the brand's light variant (page logo, favicon.svg and favicon.ico, touch and app icons) and the dark variant's logo on dark pages, copied from `brand/variants/` (D63).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The site builds with the light and dark logos and the new favicons, and the old `logo.png` and favicon are gone from the site's assets: a job
- A2 [artifact] brand/README.md says the site serves the variants: `brand/README.md`

## Contracts

None.

## Decisions

- Lead, 11-11: owner's go (D63).

## May land early

Everything.
