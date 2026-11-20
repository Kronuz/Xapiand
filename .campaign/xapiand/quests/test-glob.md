# test-glob: Linked unit tests register by file name, not by a CMakeLists.txt list

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status test-glob`.

## Outcome

Every `tests/test_*.cc` that links Xapiand code is built and registered by its file name: CMakeLists.txt globs them (CONFIGURE_DEPENDS) minus the pure-unit list, `test_stubs.cc` and `test_text.cc`, instead of listing each name. A quest that adds a linked test no longer edits CMakeLists.txt, so quests stop overlapping on it and on `tests/**`.

## Acceptance criteria

- A1 The same test set is registered: `ctest -N` names before and after are identical (a job printing both)
- A2 A new `tests/test_<name>.cc` is built and run with no CMakeLists.txt edit (a job that adds a throwaway test in its checkout and sees it in `ctest -N`)
- A3 The build-test check passes, and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
