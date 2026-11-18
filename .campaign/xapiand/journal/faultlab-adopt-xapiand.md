# faultlab-adopt-xapiand: journal

State: waiting on jobs 261118-063444-7192ac 261118-063529-314e40 261118-063447-891e3d
Quest: .campaign/xapiand/quests/faultlab-adopt-xapiand.md (the lead's; never edit it)

## Doing

Done at xapiand:a835a31c0: run.sh pins faultlab a452a8c into harness/cluster/.faultlab (gitignored,
cloned from FAULTLAB_REPO, default GitHub); history/proxy/selftest deleted, no shim. cluster.py
passes the Recorder to Mesh as its fault log; checklist.py uses load_log + check(faults=), and closes
the cluster before the recorder so a forward task's failure surfaces. ruff, pyrefly clean.

## Next

1. Read the jobs; A1 = selftest 7192ac + cluster run ed639b (3 nodes, membership,history); A2 = grep 891e3d.

## Blocked on

Nothing.

## Rulings I made

- Jobs use FAULTLAB_REPO=local faultlab checkout (no network assumed); the cluster job builds xapiand at its own commit (ed639b, home binary, cancelled).
- pyrefly search-path points at .faultlab/python, so pyrefly needs one run.sh call first.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261118-063444-7192ac, 261118-063529-314e40 |
| A2 | job 261118-063447-891e3d |
