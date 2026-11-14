# FreeBSD validation

The `FreeBSD 14 Release + tests` job builds Release with `BUILD_TESTS=ON`
and runs all registered CTest cases in a FreeBSD 14.3 amd64 VM. It runs on
pushes to `master`, `quest/**`, `quick/**`, and `codex/**`, excluding pushes
whose changes are confined to `.campaign/`. Tag and manual package builds
retain their existing FreeBSD 14.2 packaging steps.

```sh
pkg install -y cmake ninja git pkgconf zstd asio
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON -DLTO=OFF -DASIO_INCLUDE_DIR=/usr/local/include
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --parallel 4 --timeout 1800
```

The job has a 120-minute limit and preserves CTest logs on failure. Docs E2E
is deferred until a first build establishes the remaining runtime budget;
there is no measured FreeBSD E2E result yet.

## Results

YAML syntax passes Ruby's YAML parser:

```sh
ruby -e 'require "yaml"; YAML.load_file(ARGV[0])' .github/workflows/freebsd.yml
gh run list --repo Kronuz/Xapiand --branch codex/freebsd-ci \
  --json databaseId,status,conclusion,url
```

The branch is pushed at Xapiand:ebe543e8b, but the latter command returns
an empty list as of November 14, 2026, 01:00 UTC. The GitHub API confirms
Actions are enabled and all actions allowed; the remote branch contains
the expected workflow. No run URL exists yet, and the reason for the
missing push run is unknown. No FreeBSD compiler or test failure is
observed because no FreeBSD build has run. CI and portability validation
remain incomplete; no test is claimed to pass.
