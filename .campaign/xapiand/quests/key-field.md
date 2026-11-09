# key-field: Remove key prefix semantics

Risk: high. History: this landed before the campaign, under the earlier campaign skill, at strata:cef4e59 (2026-10-28); its ledger `docs/status/key-field.md` is the record. Its machine state is in `state.json`: `campaign status key-field`.

## Outcome

Keyed indexes now use an explicit manifest flag, and keys may contain any 1 to `max_term_bytes` bytes. The old prefix option and associated checks are removed.

## Acceptance criteria

Carried from the ledger, each shown by the ledger as an artifact.

- A1 [artifact] `index_options::keyed` replaces `key_prefix`; callers, checks, and plugins drop prefix; pre-freeze version bumps without shims: `docs/status/key-field.md`
- A2 [artifact] Keyed status persists through open; keys may contain 1 to `max_term_bytes` bytes: `docs/status/key-field.md`
- A3 [artifact] Tests cover every first byte through replace, erase, versions, flush, reopen, merge, and scan; mutants and revert checks cover rules: `docs/status/key-field.md`
- A4 [artifact] Fixtures and digests are re-pinned and proven to differ only in format bytes: `docs/status/key-field.md`
- A5 [artifact] Release, ASan+UBSan, and TSan suites pass; manifest decoder is fuzzed; kv rows are screened against the regression report: `docs/status/key-field.md`
- A6 [artifact] CI is green on all 8 jobs; prospect branch matches: `docs/status/key-field.md`

## Contracts

None recorded: landed work; its interfaces are in the code and the docs the journal's Result names.

## Decisions

- Per D8, a manifest lacking the `keys` bit is a valid keyless index; configured bits are set only at create and install.
- Risk high: Changes manifest format and removes existing user-visible key-prefix behavior (the lead's mapping).

## May land early

None.
