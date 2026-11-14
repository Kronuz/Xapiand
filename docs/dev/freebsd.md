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

The job has a 120-minute limit and preserves CTest logs on failure. The initial Release build and all 39 tests finish in about 16 minutes
inside the 120-minute job budget. The job also runs the docs E2E layer
with `RUN_ALL_STRICT=1` so missing prerequisites fail the job. Bash,
Python, curl, Node 24, and Newman 6 are installed inside the VM.

## Results

YAML syntax passes Ruby's YAML parser:

```sh
ruby -e 'require "yaml"; YAML.load_file(ARGV[0])' .github/workflows/freebsd.yml
gh run list --repo Kronuz/Xapiand --branch codex/freebsd-ci \
  --json databaseId,status,conclusion,url
```

[CI run 43537931113](https://github.com/Kronuz/Xapiand/actions/runs/43537931113)
passes at Xapiand:db6a58275: Release builds and **39/39 CTest cases pass**
on FreeBSD 14.3 amd64. CTest reports 40.21 seconds; the entire job spans
15 minutes 38 seconds. No FreeBSD compiler fixes or failing tests are
needed. The package job is correctly skipped for the branch push.
[Test logs](https://github.com/Kronuz/Xapiand/actions/runs/43537931113/artifacts/14090829562)
are retained by the workflow.

The docs E2E extension awaits a new CI run; the initial green run does not
exercise that layer.
