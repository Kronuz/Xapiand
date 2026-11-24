# hunt-htm-constexpr-start: journal

State: ready for review
Quest: .campaign/xapiand/quests/hunt-htm-constexpr-start.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline job 261123-132523-45413e at htm:c4577b7 (htm.h-only TU): __StaticInit 192 B, __bss 240 B, 1 __mod_init_func.
Inline variables (htm:dcc43b8) proved wrong (probe 261123-133521-fde1c8: 277 B guarded init per TU, b+c 565 B, 2 entries; before 261123-133516-e862f4).
Lead ruled the fallback: tables `extern` in htm.h, defined once in htm.cc (already in the htm library target), htm:e51e63e.
Jobs at the extern fix: probe3 (adds htm.o and a b+c+htm.o link) before 261123-135636-c6b4ab (c4577b7), inline 261123-135639-512858 (dcc43b8),
extern 261123-135642-9c642a (e51e63e); A1 build-test 261123-135645-812a48; A3 values test 261123-135647-01d709 (the dcc43b8 run 261123-133531-7b7420 was superseded).
Superseded jobs: 261123-133529-8903d0 (build-test at dcc43b8).

## Next

1. The lead reads the diff (low risk) and lands on CI green.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261123-135645-812a48 (the build-test check, passed at e51e63e43d72) |
| A2 | `docs/start-tables.md`: "576 B, 3 initializers" |
| A3 | 261123-135647-01d709 (passed at e51e63e43d72) |
| A4 | CI at the landing commit |

Jobs examined: probes 261123-155842-885328 (htm:c4577b7) and 261123-155846-9f4fa2 (htm:e51e63e) give the A2 numbers;
261123-135642-9c642a failed only because probe3's grep exits 1 under set -e when the extern header-only object has
none of the sections (the expected result); probe4 tolerates it. 261123-135636-c6b4ab, 261123-135639-512858 and
261123-133521-fde1c8 measured the before and the rejected inline-variable try; 261123-133529-8903d0 and
261123-133531-7b7420 ran at the superseded dcc43b8.

## Result

`start_vertices` and `start_trixels` are declared `extern` in htm.h and defined once in htm.cc (htm:e51e63e).
Measured with clang++ -O2 on arm64 macOS (jobs 261123-155842-885328 before, 261123-155846-9f4fa2 after):

| Object | htm:c4577b7 (defined in the header) | htm:e51e63e (extern) |
|---|---|---|
| a TU that includes htm.h | 192 B __StaticInit, 1 initializer | none |
| htm.o | 192 B, 1 initializer | 192 B, 1 initializer |
| two users + htm.o, linked | 576 B, 3 initializers | 192 B, 1 initializer |

The inline-variable try (htm:dcc43b8) was worse: 277 B of guarded init per TU (261123-133521-fde1c8).
Open: a constexpr `Cartesian` constructor (cartesian repo) would remove the last initializer; a global constructor in
another TU reading the tables now has no order guarantee (Xapiand reads them only at query time, polygon.cc:617).
Written up in htm's docs/start-tables.md; ARCHITECTURE.md names where the tables live.

## Rulings I made

Lead: extern tables over inline variables, on 261123-133521-fde1c8.
