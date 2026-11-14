# Python sortable codec

`xapiand/sortable_codec.py` provides `encode(value)` and `decode(data, kind)`, where `kind`
is `int` or `float`. Integers cover [-2**63, 2**64); bool is rejected, matching C++.
Malformed or noncanonical bytes and inexact integer conversion raise `ValueError`.
Float conversion rounds large exact integers to nearest even, matching `number::to_double`.
Zero and NaN are canonical, so signed zero and NaN payloads are not preserved.

The implementation uses only the MIT `Kronuz/sortable` header and reference model. The legacy
GPL client module and old C++ codec implementation were not opened or imported. The harness
loads the new file directly, avoiding legacy package initialization.

## Identity

At `Xapiand:f4b0ee4fa` against `sortable:ac13b77`, verification passed:

1. All 113,491 fixture lines encode identically and decode to the original typed value.
2. All 39 reject witnesses raise `ValueError` for both requested kinds.
3. One million seeded values (seed 611, integers and arbitrary binary64 bit patterns) match
   the independent C++ CLI for encoding and decoding. C++ canonical integer conversions and
   float conversions are compared as well as the original typed round trips.
4. Bool, unsupported value types and out-of-range integers are rejected.
5. Ruff reports no findings; Pyrefly strict reports zero errors for the codec and both harnesses.

The fixture SHA-256 values are
`c4d4a425c09e6d3ec49995f31e79a6bde62cc258cae6a1bbd4e0be03042f98c8` (`vectors.txt`) and
`a679d215ec843a0fbdd711c9d74232834c1d1af6529638e3725234f6fdeb7213` (`reject.txt`).

From this Python-client directory, with `SORTABLE=/Users/kronuz/code/Kronuz/sortable` and
`CLI=/Users/kronuz/code/Kronuz/.scratch/codex/sortable-speed/build/sortable_cli`:

```sh
quiet check
quiet light c6-identity 10 'Python and C++ codec identity' -- /usr/local/bin/python3 -I tests/check_sortable_codec.py "$SORTABLE" "$CLI"
/usr/local/bin/ruff format --check xapiand/sortable_codec.py tests/check_sortable_codec.py tests/bench_sortable_codec.py
/usr/local/bin/ruff check xapiand/sortable_codec.py tests/check_sortable_codec.py tests/bench_sortable_codec.py
/usr/local/bin/pyrefly check -j1 -c pyrefly.toml xapiand/sortable_codec.py tests/check_sortable_codec.py tests/bench_sortable_codec.py
```

The existing `pyrefly.toml` scopes strict checking to the independent module; the legacy
client is outside that scope. Verification used `/usr/local/bin/python3`, Python 3.15.2.

## Speed

Encode/decode values per second remain unmeasured. C6 is partial until an isolated run fills
these results. The benchmark reads the complete fixture, warms once and emits seven samples:

```sh
quiet strict c6-speed 10 'Python codec encode/decode throughput' -- /usr/local/bin/python3 -I tests/bench_sortable_codec.py "$SORTABLE" 3
```

On this x86_64 macOS host strict isolation waits for existing light holds. The x7 campaign had
long-running holds, so this task did not pause it or publish timings from a shared run.
This branch is intended for integration inside sortable-encoding; it does not replace the
legacy client module or land independently.
