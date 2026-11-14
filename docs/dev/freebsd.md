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

No FreeBSD build has run locally. The workflow requires a branch push to
start its VM. CI results and any portability failures are pending; no test
is claimed to pass yet.
