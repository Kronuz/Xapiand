# stopword-lists: Stopword lists built in, overridable, fingerprinted per index

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status stopword-lists`.

## Outcome

Stop-word lists never silently disappear (D137). The 18 shipped languages are compiled into the binary as perfect-hash sets; `<data path>/stopwords/<lang>.txt` overrides a language and custom names keep working; each index records the fingerprint of every list it stops with and refuses when that list is missing or differs. Today `getStopWords` (src/stopper.cc:85) returns nullptr for a missing file and callers then stop nothing.

## Acceptance criteria

- A1 With no data path and no stopwords directory, every shipped language stops exactly the words of its shipped list (a test per language compares against the list in the tree), and start-up reads no stopword file
- A2 A file at `<data path>/stopwords/<lang>.txt` replaces that language's built-in list, and a custom-named list loads from the data path; tests check the words stopped, not only that a list loaded
- A3 An index records the fingerprint of each list it used at index time; reopening or querying it with that list missing or changed fails with an error naming the index, the list and both fingerprints; tests cover missing, changed and unchanged, and the refusal test fails with the check reverted
- A4 No path maps a missing list to "no stopping": a test with an unknown language name gets the refusal or a 400 naming the language, never a silent pass
- A5 Start-up cost and memory measured before and after (strict job): honest numbers, a loss stated
- A6 [artifact] User docs describe built-in lists, overrides and the refusal; the changelog names the behavior change under [Unreleased]
- A7 The suite passes and CI is green

## Contracts

Index metadata gains the list fingerprints (a persisted key; formats are open until release, per the formats ruling). `getStopWords` no longer returns nullptr for a known language. Callers: src/engine/text.cc:295 and src/multivalue/generate_terms.cc:534 (the latter is outside this file set: request it from the lead if it must change).

## Decisions

- D137 (owner 2026-11-19). Use the constexpr-phf library if it fits; otherwise a sorted array, measured.

## May land early

The built-in lists and their tests (A1) behind the existing loader, before the refusal changes behavior.
