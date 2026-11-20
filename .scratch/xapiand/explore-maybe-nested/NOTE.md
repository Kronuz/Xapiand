# explore/maybe-nested: `a MAYBE (b MAYBE c)`

## Q1 Confirmed on master (by trace; test job below)
Postfix is `a b c MAYBE MAYBE`. The first MAYBE builds M(b,c). At the second, last_op is Maybe, the top (`ob`) has
`_and_maybe`, so the "append to the top's list" branch pushes `a` onto it: `{"_and_maybe":[b,c,a]}`
(src/query_dsl.cc:1174-1180). Required is b, not a.
- Built tree: matches every doc with b (a and c only boost).
- Intended `a AND_MAYBE (b AND_MAYBE c)`: matches every doc with a.
- Differs on docs with b and not a (wrongly returned) and docs with a and not b (wrongly dropped).
Same for `(a MAYBE b) MAYBE (c MAYBE d)`: built [c,d,M(a,b)], c required.

## Q2 Fix
In the Maybe case delete the `ob` branch (the top, i.e. the RIGHT operand, already holds `_and_maybe`: merge into it).
A right operand is never merged for MAYBE; the left-operand branch (`ob2`) stays, so `a MAYBE b MAYBE c` stays flat
[a,b,c] and a grouped right operand becomes one nested optional clause.

## Q3 Other operators
AND/OR/XOR have the same right-operand branch, but they are associative and commutative, so
`a AND (b AND c)` -> AND[b,c,a] matches the same documents (order only; scoring unaffected). Not a defect.
NOT is unary (create_exp_op_dsl), no merge. The lexer has no AND_NOT token. Mixed nesting (`a MAYBE (b AND c)`)
does not merge because last_op differs from the operator. Only MAYBE (ordered, a-required) is wrong.
Note: `last_op` is a single global, so a merge needs the operands adjacent on the stack; they are, postfix guarantees it.

## Evidence
Test: tests/test_query_dsl_not.cc test_maybe_keeps_a_grouped_right_operand_optional (tree-shape check).
Jobs: see commit/journal ids appended below.
Jobs: fix 261120-002143-79a9e4 (expect pass); mutant (master query_dsl.cc, expect fail) 261120-002150-dd82b5.
