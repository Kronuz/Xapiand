# AGENTS.md: working in Xapiand

Xapiand is a C++20 distributed search engine. [ARCHITECTURE.md](ARCHITECTURE.md) says what it is;
this file says how to work on it. The search engine is the [strata](https://github.com/Kronuz/strata)
library behind the seam in `src/engine/`; generic code lives in the `Kronuz/*` libraries, fetched by
CMake `FetchContent`. A bug in a library is fixed in its own repository, not here.

## Current campaign

The plan, quests and their evidence are in `.campaign/xapiand/`, run by the `campaign` skill, which is
authoritative for process: `campaign map` shows the whole campaign, `campaign status` what each quest
still needs. Builds, tests and benchmarks are `campaign job submit` jobs. `docs/status/` holds the
earlier skill's ledgers, kept as history.

## Hard rules

- Never read GPL sources: `src/xapian/**`, `src/sortable_serialise.*`,
  `contrib/python/xapiand-py/xapiand/utils/sortable.py`.
- Public repositories under `github.com/Kronuz/`: author and commit as
  `Germán Méndez Bravo <german.mb@gmail.com>`.
- No AI attribution in commits, PRs, code or docs: no `Co-authored-by`, session links or "Generated with".
- Copyright line for new files: "Copyright © 2026 Germán Méndez Bravo (Kronuz)".

## Build and test

CMake with Ninja only; never `make`. CI's `.github/workflows/ci.yml` is the reference:

```sh
cmake -GNinja -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF
ninja -C build
ctest --test-dir build --output-on-failure
BIN="$PWD/build/bin/xapiand" harness/run_all.sh e2e tls   # docs-as-E2E, needs newman
```

One test: `ninja -C build xapiand_test_NAME` builds `tests/test_NAME.cc` (the target carries the
`xapiand_` prefix), and `ctest --test-dir build -R '^test_NAME$'` runs it. A test that must fail without a
fix is built and run at the code without it, never assumed to fail.

Sanitizers (`-DASAN=ON -DUBSAN=ON`, `-DTSAN=ON`) are CI's job: push the branch. Toggles, test layers and
the harness are in [docs/dev/testing.md](docs/dev/testing.md).

## Sharing this machine

Other agents and benchmarks run on this machine; a compile during a measurement corrupts it. Run
`quiet check` before compiling, testing or measuring. Wrap builds and tests in
`quiet light NAME MINUTES "what" -- nice -n 19 ninja -C build -j4` (at most 4 jobs) and measurements in
`quiet strict`. Exit 1 means do not compile; exit 2 means a light run queues for its slot.

## Where things live

- `src/` is Xapiand's own code; the entry point is `src/main.cc`, orchestrated by `XapiandManager`
  (`src/manager.*`). Library seams are `src/xapiand_*_trace.h`.
- `docs/dev/` holds developer pages; [docs/dev/agent-codebase.md](docs/dev/agent-codebase.md) has the code
  map, conventions, load-bearing invariants and traps. `docs/design/` and `docs/reviews/` hold designs
  and their reviews; `docs/src/content/docs/` is the user-facing site and feeds the docs-as-E2E suite.
- `docs/discussions/` is a vendored copy of the Kronuz Discussions package, byte-identical with
  `~/code/KronuzBlog/Kronuz.github.io/discussions` and `~/code/KronuzBlog/gmendezb-pages/discussions`.
  Change one source tree, sync the whole directory, verify all three copies.
- The changelog is `docs/src/content/docs/changelog.md` (`CHANGELOG` links to it), curated and user-facing.
