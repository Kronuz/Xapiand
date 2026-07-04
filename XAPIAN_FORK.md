# XAPIAN_FORK.md — the vendored Xapian fork

The `src/xapian/` tree is **not pristine upstream Xapian**. It's a *fork*: a
pristine snapshot of a tagged Xapian release, plus a small stack of deliberate
patches that Xapiand needs. The root [AGENTS.md](AGENTS.md) tells you not to
casually refactor vendored code — that still holds. This file is the exception
manual: what the fork is, why each patch exists, and how to move it to a new
Xapian version without losing the patches.

If you're just tracing a call *into* Xapian, you don't need any of this. You
need it when you're (a) bumping the Xapian version, or (b) adding/adjusting one
of our patches.

## The shape: pristine snapshot + patches

History here is deliberately two layered:

1. **A pristine-snapshot commit** — the untouched source of one tagged Xapian
   release, massaged only by mechanical include-flattening (see below). These
   are the floor you rebuild patches on top of. Find them with:
   ```
   git log --oneline -- src/xapian | grep 'xapian-core v'
   ```
   Known snapshots: `5c6248ad2` = v1.5.0 (dev), `b379bd0b1` = v2.0.0.
2. **Feature patches on top** — one commit per feature, each with a detailed
   "what / why / need" message (that message *is* the documentation for the
   patch; upstream's terse originals were the thing we regretted). Every patch
   subject is prefixed `xapian(<area>): …`.

**Rule:** a pristine snapshot commit contains *only* upstream code (plus the
include rewrite). Never fold a patch into it. Never edit a snapshot commit to
"fix" something — add a patch commit instead. This is what makes the next
version bump a clean rebase instead of an archaeology dig.

## How the pristine snapshot is made

Xapian normally builds via autotools/configure. We don't run configure; we
compile the sources directly (next section). So a snapshot is the source dirs
copied out of `xapian-core@<tag>`, with three hand steps:

1. Copy the source subdirs we compile (`api backends cluster common expand
   geospatial languages matcher net queryparser unicode weight` + the honey/
   glass/inmemory/multi backends), plus `generate-exceptions`,
   `exception_data.pm`, and the public headers (`include/xapian/*.h` →
   `src/xapian/`, `include/xapian.h` → `src/xapian.h`), and `COPYING` → `LICENSE`.
   Dirs upstream dropped (e.g. `diversify/` in 2.0.0) just don't get copied.
2. Create `src/xapian/version.h` **by hand** at the release version. configure
   normally generates it; we pin it (e.g. `XAPIAN_VERSION "2.0.0"`, and the
   `XAPIAN_MAJOR/MINOR/REVISION` and remote-protocol version macros to match).
3. Run `./fix_xapian_includes.py` (repo root) to flatten upstream's
   `#include <xapian/foo.h>` layout to our vendored layout. **Always run this
   after copying pristine source.**

Commit that as `xapian-core v<version> (git@<upstream-sha>) [https://github.com/xapian/xapian]`.

## How it's compiled (no configure)

`CMakeLists.txt` (repo root) GLOBs this tree into an object library
`XAPIAN_OBJ`. Relevant bits, around the `file(GLOB XAPIAN_SRC_LIST ...)`:

- The GLOB pulls `src/xapian/*.c*`, and each subdir (`net/*`, `backends/remote/*`,
  `matcher/*`, `queryparser/*`, …). **A whole dir being globbed means every
  file in it must compile** — you can't leave a half-ported file around.
- `generate-exceptions` + `exception_data.pm` are run at build time to generate
  `error.h`/`errordispatch.h`. Add a new exception class by editing
  `exception_data.pm` (not a hand-written header).
- Snowball stemmers (`languages/*.sbl`) and the lemon query-parser grammar
  (`queryparser/queryparser.lemony`) are code-generated at build time too.
- `version_h.cc` and `queryparser/lemon.c` are explicitly *removed* from the
  glob (`list(REMOVE_ITEM …)`).

Build just Xapian-affecting changes with the normal `cd build && make xapiand -j4`.
There's no separate Xapian build; it's one object library linked into xapiand.

## The patches we carry (on top of v2.0.0 `b379bd0b1`)

Each is a standalone commit; read its message for the full rationale. Grouped:

**Storage / API niceties**
- `xapian(glass): don't store empty termlist tags` — `del()` the termlist entry
  for a document with no terms instead of writing an empty tag. Xapiand indexes
  many value-only docs, so empty termlists are common, not an edge case.
- `xapian(glass): don't compress the document-data table` — DOCDATA compress
  min → 0. Our records are already-compact MessagePack on the query hot path;
  decompression cost wasn't worth the marginal size win.
- `xapian(glass): create the full database directory path (mkdir -p)` — Xapiand
  creates shard dirs at arbitrary depth on demand; a single `mkdir()` fails ENOENT.
- `xapian(api): expose Database::refs()` — read the intrusive refcount so the
  shard pool can assert unique ownership at check-in.
- `xapian(api): add DatabaseNotAvailableError and DocVersionConflictError` —
  Xapiand distinguishes "shard momentarily unavailable" (retry/failover) and
  optimistic-concurrency version conflicts, which upstream doesn't model.

**Core semantics**
- `xapian(core): internal_intrusive_ptr with safe-move handle semantics` — see
  "Pointers" below. **Do not "simplify" this back to `intrusive_ptr_nonnull`.**
- `xapian(api): return DocumentInfo from writes` — add/replace_document return
  `{ docid, rev version, string term }` instead of `docid`/`void`, so Xapiand
  gets the version (MVCC / optimistic concurrency) and resolved term back
  atomically. The remote reply becomes `REPLY_ADDDOCUMENT` carrying all three.

**Remote / cluster** (all must stay 1:1 with `src/server/remote_protocol_views.cc`)
- `xapian(remote): database directory selection (MSG_READACCESS)` — a remote
  server can front many shards and the client picks one by path per connection.
  This is load-bearing for Xapiand's one-server-many-shards model.
- *(in progress at time of writing)* remote get_revision, remote no-stats-cache,
  remote KeyMaker, and the two-phase split matcher. See the migration section.

Patches known to be **upstreamed / dropped** in 2.0.0 (do not re-port):
`intrusive_ptr_nonnull` + `database_factory` (2.0.0 has them), `DatabaseNotFoundError`
on ENOENT (2.0.0 does it), `[[fallthrough]]` (2.0.0 is C++17 throughout), the old
CJK tokenizer patches (2.0.0 replaced CJK with ICU word-breaking, so the files
they touched no longer exist).

## What changed 1.5.0 → 2.0.0 (findings, so you don't re-derive them)

- **Remote protocol is additive, v44 → v47.** New MSG/REPLY types
  (RECONSTRUCTTEXT, the SYNONYM* family, REQUESTDOCUMENT, WDFDOCMAX); none
  removed or renumbered. So our custom messages (e.g. `MSG_READACCESS`) append
  cleanly — the base messages our patches touch are stable.
- **Pointers changed per-type; don't blanket-convert.** 2.0.0 uses
  `intrusive_ptr_nonnull` for value-class internals. Its move ctor does
  `rhs.px = 0` — a *moved-from handle goes NULL despite the "nonnull" name*.
  Fine single-threaded; a latent null-deref in Xapiand's coroutine/pool code,
  which observes moved-from handles. Our `internal_intrusive_ptr<T, Owner>`
  refills moved-from (and default-constructed) handles with a fresh empty
  internal instead, so they stay usable. Match 2.0.0's type on each pointer
  *unless a patch deliberately changes it* (this one does, for Database,
  Document, MSet, ESet, QueryParser, TermGenerator, Registry, cluster types).
  Because it's default-constructible, we also drop 2.0.0's `database_factory()`
  helper and assign `internal =` in the fd ctor body (fixing an original
  fall-through so each backend case `return;`s).
- **Write path takes `std::string_view`.** replace_document(term) and friends
  are `string_view` now; combine with our `DocumentInfo` return type.
- **KeyMaker registry modernized to intrusive_ptr + `release()`.** Prefer
  2.0.0's model and adapt Xapiand callers; keep our `clone()` (Xapiand's
  handler.cc uses `sorter->clone()` directly and needs it).
- **Matcher was refactored heavily.** New `EstimateOp`/`resolve()` estimate
  framework, sorter passed as a pointer, `Matcher` ctor lost
  `full_db_has_positions` (drop it everywhere, including Xapiand app code) and
  `merge_stats` gained a `(ptr, collapse)` signature with explicit stats-object
  merging. Our two-phase split (prepare → merge, for correct distributed remote
  matching) must be **rebuilt on 2.0.0's matcher**, not merged into it.
- **`EmptyDatabase::end_transaction_` removed**, glass uses `not_present()`
  instead of `at_end()` after a false `throw_if_not_present`.

## The 1:1 invariant: remoteserver.cc ↔ remote_protocol_views.cc

Xapiand has its *own* server-side remote implementation in
`src/server/remote_protocol_views.cc`, and uses xapian's `RemoteDatabase`
(client side) to talk to it. The two speak the same wire protocol and **must
stay message-for-message in sync**. If you touch a MSG/REPLY here (add one,
change a payload), make the mirror change there, and vice-versa. The remote
patches above were designed as matched pairs across these two files.

## Upgrading to a new Xapian version (the procedure)

1. Get the target `xapian-core` at its tag. Build the pristine snapshot commit
   per "How the pristine snapshot is made" on a fresh branch off the current
   fork tip's *base* (or off the previous snapshot). Commit it pristine.
2. Reconcile each patch on top, oldest first. Two workable mechanics:
   - **cherry-pick in dependency order** (`git cherry-pick -x <patch>`), resolving
     conflicts — best for keeping per-feature commits and messages; or
   - a **per-file 3-way merge** (`git merge-file`, base = old pristine, ours =
     old fork tip, theirs = new pristine) to see the whole delta at once — good
     for triage, but it collapses features and can *silently duplicate* code
     when both sides add near each other (it did, in `keymaker.h`), so anything
     it produces still needs a real build.
   For each patch decide: still needed (port), upstreamed (drop), or the code it
   touched is gone (drop/redesign). Record the call in the commit message.
3. Rebuild features that upstream reshaped from under us (matcher, keymaker)
   *on top of* the new code rather than force-merging the old shape.
4. `make xapiand -j4`, fix iteratively — the compiler catches signature drift
   and any silent merge duplications. Then E2E (`harness/e2e_capture.sh`) and
   the graceful-shutdown / benchmark harnesses.
5. Keep `src/server/remote_protocol_views.cc` in sync (see the invariant).

## Gotchas

- **`git cherry-pick -n` leaves no sequencer state**, so `git cherry-pick
  --abort` is a no-op afterwards. To undo a half-applied `-n` pick, use
  `git reset --hard HEAD` (commit your good work first).
- **Run `./fix_xapian_includes.py`** after any pristine copy; forgetting it
  leaves upstream-style includes that don't resolve in our layout.
- **`version.h` is hand-maintained** (configure would generate it). Bump it in
  the snapshot commit.
- A whole subdir is GLOBbed, so **every file in a touched dir must compile** —
  you can't stash a half-ported file in the tree.
- Public-repo identity rules apply to these commits like any other
  (`Germán Méndez Bravo <german.mb@gmail.com>`); credit upstream/other authors
  in the body, not the author field.
