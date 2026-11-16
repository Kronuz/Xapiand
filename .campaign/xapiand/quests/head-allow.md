# head-allow

## Outcome

HEAD answers with the headers and Content-Length a GET would carry and no body, a missing document's
404 included (RFC 9110 9.3.2); the `Allow` header lists the verbs Xapiand routes, and STORE, which has
no route, is gone (hunt list; docs-site-refresh-reference's Blocked on 5).

## Acceptance criteria

- A1 `test_head_has_no_body_and_options_lists_the_routed_verbs` in tests/test_node.cc passes at the
  head and fails without the fix: the `suite` check and a `--fails-without` job
- A2 The suite and the docs E2E pass: the `suite` and `e2e` checks

## May land early

None.
