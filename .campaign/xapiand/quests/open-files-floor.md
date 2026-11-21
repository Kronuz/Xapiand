# open-files-floor: A start with fewer open files than the full pool needs shrinks the pool and starts

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
is in `state.json`: `campaign status open-files-floor`.

## Outcome

`adjustOpenFilesLimit` (src/main.cc) refuses a start only below the minimum that holds one database
and one client (75 descriptors plus those the process has open). Before, it took the minimum from
the full configured pool (1471 for a pool of 200 and 1,000 clients), so `--max-files 300` logged
"database_pool_size reduced to 34" and then exited 72. A limit between the two now starts with the
reduced pool and the clients the remaining descriptors allow; limits at or above the old minimum
size the pool and clients as before. From the hunt row "Start refuses any open-files limit below
1471" (jobs 261119-004113-82ceb7, 261119-012453-e1a2b8).

## Acceptance criteria

- A1 At the minimum the start holds one database and the clients the rest allow, and one
  descriptor under it is refused before any reduction message: tests/test_coverage_main.cc and
  harness/coverage_e2e_main.sh, failing at master's src/main.cc and passing at the fix (explorer
  jobs 261121-052449-14ef12 at master, 261121-084557-0900f0 and 261121-084600-7d2eb3 at the fix,
  rerun at the landed code)
- A2 The suite passes at the landed code: the `suite` check

## Contracts

None: a start that exited 72 now runs with a smaller pool; no format or API changes.

## Decisions

- Lead, 2026-11-21: the floor is one database and one client, not the full pool.

## May land early

None.

## Tasks

None: the fix and tests are explore/xapiand/open-files-floor (Xapiand:98b29aa14..ae38902f7); the
contributor merges it into the quest branch.
