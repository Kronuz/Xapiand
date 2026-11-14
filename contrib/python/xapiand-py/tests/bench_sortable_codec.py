"""Run under quiet strict: bench_sortable_codec.py SORTABLE_ROOT [PASSES]."""

from __future__ import annotations

import importlib.util
import platform
import sys
from pathlib import Path
from time import perf_counter_ns

MODULE = Path(__file__).resolve().parents[1] / "xapiand" / "sortable_codec.py"
spec = importlib.util.spec_from_file_location("sortable_codec", MODULE)
assert spec and spec.loader
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)


def main() -> None:
    root = Path(sys.argv[1])
    passes = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    if passes < 1:
        raise ValueError("passes must be positive")
    values: list[int | float] = []
    for line in (root / "test" / "vectors.txt").read_text().splitlines():
        kind, text, _ = line.split()
        values.append(float(text) if kind == "d" else int(text))
    encoded = [codec.encode(v) for v in values]
    kinds = [float if isinstance(v, float) else int for v in values]
    print(platform.platform(), sys.version)
    print("sample,values,encode_per_second,decode_per_second")
    for sample in range(-1, 7):
        start = perf_counter_ns()
        for _ in range(passes):
            for value in values:
                codec.encode(value)
        middle = perf_counter_ns()
        for _ in range(passes):
            for data, value_kind in zip(encoded, kinds, strict=True):
                codec.decode(data, value_kind)
        end = perf_counter_ns()
        if sample >= 0:
            count = passes * len(values)
            print(
                f"{sample},{count},{count * 1e9 / (middle - start):.1f},{count * 1e9 / (end - middle):.1f}"
            )


if __name__ == "__main__":
    main()
