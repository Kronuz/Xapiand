# hunt-tls-expiry: Xapiand calls reactor's seconds_to_expiry and drops its own

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-tls-expiry`.

## Outcome

Hunt row 368. `src/tls.cc`'s `tls_seconds_to_expiry` (tls.cc:78) duplicates reactor's
`reactor::tls::seconds_to_expiry(asio::ssl::context&)` (reactor_tls.h:380), which returns
`std::optional<long>`. Xapiand calls reactor's and deletes its own; each caller handles the empty
result the way `0` was handled before. The reduction is recorded in `docs/dev/extraction-ledger.md`.

## Acceptance criteria

- A1 The suite passes at the change: the `suite` check
- A2 No caller changes behavior: a certificate expiring within 14 days still logs the warning, and
  one with no presented certificate still yields what it did. A test or the existing TLS tests show
  it, named in the journal
- A3 [artifact] `docs/dev/extraction-ledger.md`: the row for this reduction

## Contracts

Consumes reactor's `tls::seconds_to_expiry`.

## Decisions

- Lead, 2026-11-21: low risk; one contributor.

## May land early

None.
