# Codebase guide for contributors

Detail moved out of AGENTS.md: where code lives, conventions, invariants and traps.

## Where things live

Documentation outside the root's README, AGENTS, and ARCHITECTURE goes under `docs/`:
designs in `docs/design/`, their architect reviews in `docs/reviews/`, and developer guides
(testing) in `docs/dev/`. The user-facing site is `docs/src/`, and only
`docs/src/content/docs/` feeds the docs-as-E2E suite.

| You're touching… | Start in |
|---|---|
| On-disk format, durability, volumes | `src/database/{shard,shard_group,command,data}.{cc,h}`; each index is a `strata::group` whose journal is its durability ([journal durability](../mechanisms/journal-durability.md)); blob volumes are `app.vol-N` files in the group's file set; the index files are strata's (`src/engine/strata/`) |
| Schema handling, field types | `src/database/schema*.{cc,h}`, `src/reserved/` |
| HTTP API / request handling | `src/server/search_{service,views}.*`, `src/server/http_utils.h`; transport is the [http](https://github.com/Kronuz/http) library |
| The event loop / connections | the [reactor](https://github.com/Kronuz/reactor) library; `src/manager.*` owns the services |
| Query languages | `src/query_dsl.*` (JSON/MsgPack); the string parser is the [boolean-parser](https://github.com/Kronuz/boolean-parser) library |
| Aggregations | `src/aggregations/` |
| Geospatial | `src/geospatial/`, `src/multivalue/geospatialrange.*` |
| Value encoding | `src/serialize*.{cc,h}` over [sortable](https://github.com/Kronuz/sortable), `src/length.*` |
| Logging | `src/log.h` (category switches); the logger is the [logger](https://github.com/Kronuz/logger) library |
| Scripting | `src/lua/`, `src/script.*` |
| Small utilities | top-level `src/*.hh` / `*.h` (see the table in ARCHITECTURE.md) |

Entry point is `src/main.cc`; the process is orchestrated by
`XapiandManager` (`src/manager.*`).

## Conventions you'll see everywhere

- **Logging macros `L_*`.** Logging is pervasive (~2,790 call sites). Most
  category macros (`L_CALL`, `L_DATABASE`, `L_EV`, …) compile to **nothing** by
  default and are switched on by editing the `#define`s near the top of
  `src/log.h`. To trace a subsystem, flip its category there and rebuild. Log
  arguments are evaluated lazily, so it's fine to pass expensive expressions.
- **Services on the reactor.** There is no libev and no `Worker` tree any more.
  `XapiandManager` runs a control-plane `reactor::Reactor`; the HTTP
  service runs on the [reactor](https://github.com/Kronuz/reactor)
  library's shared-nothing loops; a request is a task on its loop and device work is a chore
  on one `WorkPool`. Never block a reactor thread.
- **MsgPack as the universal value.** JSON, MessagePack, and internal objects are
  all `MsgPack` (from the [msgpack](https://github.com/Kronuz/msgpack) library), a
  copy-on-write wrapper. Reserved keys are
  `$`/`_`-prefixed (`src/reserved/`).
- **Perfect-hash dispatch.** Reserved-word handling (query DSL, aggregations) is
  done with compile-time perfect-hash tables (`phf::make_phf`), not if-ladders.
  Add a keyword by extending both the hash table and its `switch`.
- **Serialization is order-sensitive.** Values stored for range/sort use
  the `sortable` codec through `Serialize::` (memcmp order == exact numeric order,
  across integer and floating types). If you change how a value
  is encoded, you change its sort/range semantics — tread carefully and keep the
  encoding monotonic.
- **Splash banner font.** The ASCII wordmark in `banner()` (`src/main.cc`) is
  figlet's **Standard** font (the default), *not* `slant` — `figlet apiand`
  reproduces the letters after the "X" exactly. The "X" itself is hand-stylized
  (the diamond `\ \/ /` … `/_/`), not figlet output, so don't regenerate the whole
  word blind; keep the custom X and only the "apiand" tail is figlet. prism's
  banner uses the same font, straight from `figlet prism` (all-lowercase, no custom
  glyph needed).

## Load-bearing invariants (don't break these)

- **Trixel ids encode the quadtree path 2 bits per level**, which is *why* a
  region is a contiguous integer range and why geo serialization is big-endian
  (the [htm](https://github.com/Kronuz/htm) library, `src/serialize.cc`). Any change to id construction or
  endianness breaks geo indexing silently.
- **A group's apply is a function of the entry and the state it starts from**
  (`ShardApplication`, `src/database/shard_group.h`). Effects write positionally,
  so a replay or a retry rewrites the same bytes, and a blob's locator is a
  function of the log. Don't make apply read what voters can differ on (a
  clock, a file's current end, the leader's field table).
- **Prepare writes nothing.** The leader prepares a write under the index's turn;
  every voter applies the command, and strata refuses a stale `expected` version
  at apply, the same on every replica.

## Traps (things that have already bitten, per the code review)

- A write is durable when its command is in the group's journal at the ack
  level it was proposed with; `commit()` is the group's checkpoint, which
  truncates the log, not the durability point. A pool with no journal (tests,
  the CLI) is durable only at its commits.
- A locator's `raw` views its `Data`'s bytes (`src/database/data.h`): a view
  taken from it dies with that `Data` or its next `flush()`.
- `query_dsl.cc::process` recurses over the query object with no depth cap, so a
  deeply nested query can exhaust the stack — be careful adding nesting, and
  consider a guard if you're in there. (The boolean-parser library builds its tree
  iteratively; Xapiand reads its RPN queue directly anyway.)

## Making changes well

- Match the surrounding style: heavy `constexpr`, CRTP for zero-overhead
  polymorphism, `string_view` over copies, RAII for every resource (sockets,
  shard uses, turns).
- When adding a field type, reserved word, or aggregation, you'll typically touch
  three places: the reserved vocabulary (`src/reserved/`), the schema/serialize
  layer, and the perfect-hash dispatch. Grep an existing one end-to-end first.
- If you extract a utility to its own repo (the `base-x` / `uinteger_t` /
  `fantasyname` pattern), check the licensing note in ARCHITECTURE.md.
- There's no substitute for flipping on the relevant `L_*` category and watching
  the logs; the instrumentation is already there.
