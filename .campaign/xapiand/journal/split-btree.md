# split-btree: journal

State: waiting on jobs 261121-115613-5b208b 261121-115614-b70075 261121-115611-39c6ac 261121-115615-05b865 261121-115051-03e1ff (split-check not landed: split job queued anyway)
Quest: .campaign/xapiand/quests/split-btree.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done and pushed (quest/xapiand/split-btree at 4a1487d): btree.h split into 9 pieces, node_layout.cc into 6, docs/source-layout.md written. Only jobs remain.

## Next

1. Read A6 (261121-115611-39c6ac), A1 (build-test 261121-115045-ff237f), A3 (ctest -N at base 261121-115051-03e1ff vs head 261121-115043-9babb1; diff their logs).
2. split-check has not landed: when `campaign status split-check` shows landed, rerun `campaign verify split-btree` and answer its warnings.

## Blocked on

Nothing (A2 waits on split-check landing).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-115613-5b208b at 4a1487d (older 261121-115045-ff237f at 9799f4e superseded) |
| A2 | split check 261121-115614-b70075 queued at 4a1487d; the tool (split-check) had not landed. Self-check: concatenated moved bodies diff identical to the original (btree.h lines 131-3589, node_layout.cc lines 17-1064 and 1066-1087, blank lines ignored); largest file btree/btree_tree.h 898 lines |
| A3 | base 8b65a55: 261121-115051-03e1ff (intentionally at base); head 4a1487d: 261121-115615-05b865 (first base job 261121-115041-6e1780 was superseded by the head job, resubmitted with another build dir) |
| A4 | docs/source-layout.md lists every new file |
| A5 | CI on pushed branch |
| A6 | selfcontained.sh -s 17 -x btree.h/map.h/set.h job 261121-115611-39c6ac at 4a1487d; local quiet-light runs: 9 ok at c++17 and c++20 |

## Result

btree.h 3,592 lines -> 139-line include plus btree_compare 184, btree_params 269, btree_node 596, btree_iterator 139, btree_tree 898, btree_node_impl 362, btree_tree_modify 484, btree_tree_internal 328, btree_container 450. node_layout.cc 1,087 -> 43 lines plus tests/node_layout/{fixtures 54, compact_roots 144, copy_failures 291, root_growth 299, empty_insert 110, prepared 192}.

## Rulings I made

- btree_tree.h is 898 lines: the `btree` class definition cannot be split across files; it is under 1,000.
- Pieces are self-contained (A6): each includes its siblings relatively (`"btree_x.h"`, as map.h includes `"btree.h"`) and the standard headers it uses; only #include lines and the opening comment were added (commit 4a1487d).
- Test pieces stay in one TU: each opens its own `namespace {` (same anonymous namespace), `#pragma once`, included in original order, so no dynamic initializer moves and the `layout_*` targets and tests/run_layout.sh are unchanged. No shared-helper header was needed.
- CMake has no install rules (INTERFACE library on the repo root), so none to update; docs/source-layout.md is the list of pieces.
- Added a license notice and include guard to each piece (new lines, not moved ones).
