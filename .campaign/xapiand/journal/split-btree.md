# split-btree: journal

State: waiting on jobs 261121-145037-38dcba (A2, fixed checker at merged HEAD f6d0fc6)
Quest: .campaign/xapiand/quests/split-btree.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Reassessed against cpp-libraries docs/SPLITTING.md (owner refinement). Pushed at 553ed60. Only jobs remain; A7 is the lead's read.

## Next

0. Read 261121-145037-38dcba; answer each warning here (using-declaration of detail helper, dropped static, new prototype); fix any error with moves only.
1. Read A1, A3 (head 261121-121302-5dc03c vs base 261121-115051-03e1ff logs), A6.
2. When split-check lands, rerun `campaign verify split-btree`, answer split_check warnings.

## Blocked on

Nothing (A2 waits on split-check landing).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-121304-c1d1ff at 553ed60 |
| A2 | split 261121-145037-38dcba (split-check-sources checker, merged HEAD f6d0fc6); 261121-123001-f68c62 (old checker) failed on include guards, which the new one allows; replaces cee137. Self-check: multiset of moved body lines equals the original (3,114 non-blank lines); node_layout.cc byte-identical to the original |
| A3 | base 8b65a55: 261121-115051-03e1ff (intentionally at base); head 553ed60: 261121-121302-5dc03c |
| A4 | docs/source-layout.md lists every new file and the files kept whole, with why |
| A5 | CI on pushed branch |
| A6 | selfcontained.sh -s 17 over the 6 new headers: job 261121-121259-79bc8a; local quiet-light runs 6 ok at c++17 and c++20 |
| A7 | the lead's read |

## Result

btree.h 3,592 -> 137-line umbrella plus btree_compare 189, btree_params 280, btree_node 876 (class and its member definitions), btree_iterator 209, btree_tree 1,674 (class and all members), btree_container 458. tests/node_layout.cc kept whole (1,087).

## Rulings I made

- Dropped btree_node_impl.h, btree_tree_modify.h, btree_tree_internal.h: the members they held (out-of-class definitions of the class before them) are not families someone opens alone, only a line-count cut. They join btree_node.h, btree_iterator.h, btree_tree.h; slices stay in original order within each piece. btree_tree.h stays whole at about 1,670 lines (one class); the layout note says why.
- tests/node_layout.cc back to one file (byte-identical to the original): separate TUs would need linkage changes (fixtures, ThrowingValue and RootOrder shared across topics, main's sequencing) for no reader benefit; the anonymous-namespace fragments are forbidden by SPLITTING.md. Commit 8db2014.
- Pieces are self-contained (A6): each includes siblings relatively and the standard headers it uses.
- No shared helper, no moved dynamic initializer; CMake has no install rules (INTERFACE library on the repo root).
- Each piece carries a license notice, include guard and namespace (added lines).
