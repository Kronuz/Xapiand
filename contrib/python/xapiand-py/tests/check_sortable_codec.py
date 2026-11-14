"""Cross-language identity: python check_sortable_codec.py SORTABLE_ROOT SORTABLE_CLI.

Loads only the new module and the MIT C++ fixtures; the legacy client is not imported.
"""

from __future__ import annotations

import importlib.util
import math
import random
import struct
import subprocess
import sys
from pathlib import Path

MODULE = Path(__file__).resolve().parents[1] / "xapiand" / "sortable_codec.py"
spec = importlib.util.spec_from_file_location("sortable_codec", MODULE)
assert spec and spec.loader
codec = importlib.util.module_from_spec(spec)
spec.loader.exec_module(codec)


def equal(left: float, right: float) -> bool:
    return left == right or (
        isinstance(left, float)
        and isinstance(right, float)
        and math.isnan(left)
        and math.isnan(right)
    )


def main() -> None:
    root, cli = Path(sys.argv[1]), sys.argv[2]
    count = 0
    for line in (root / "test" / "vectors.txt").read_text().splitlines():
        kind, text, encoded = line.split()
        value = float(text) if kind == "d" else int(text)
        data = bytes.fromhex(encoded)
        assert codec.encode(value) == data, line
        assert equal(codec.decode(data, float if kind == "d" else int), value), line
        count += 1
    rejects = 0
    for line in (root / "test" / "reject.txt").read_text().splitlines():
        hexed = line.split()[-1]
        for kind in (int, float):
            try:
                codec.decode(b"" if hexed == "-" else bytes.fromhex(hexed), kind)
            except ValueError:
                pass
            else:
                raise AssertionError(line)
        rejects += 1
    rnd = random.Random(611)
    total = 0
    for _ in range(100):
        values: list[int | float] = []
        lines: list[str] = []
        for _ in range(10000):
            if rnd.randrange(2):
                value: int | float = rnd.randrange(-(1 << 63), 1 << 64)
                lines.append(f"i {value}")
            else:
                bits = rnd.getrandbits(64)
                value = struct.unpack(">d", bits.to_bytes(8, "big"))[0]
                lines.append(f"f {bits:016x}")
            values.append(value)
        answer = subprocess.run(
            [cli, "encode"],
            input="\n".join(lines) + "\n",
            capture_output=True,
            text=True,
            check=True,
        )
        hexes = answer.stdout.splitlines()
        assert len(hexes) == len(values)
        for value, hexed in zip(values, hexes, strict=True):
            data = bytes.fromhex(hexed)
            assert codec.encode(value) == data, (value, hexed)
            assert equal(
                codec.decode(data, float if isinstance(value, float) else int), value
            ), (value, hexed)
        decoded = subprocess.run(
            [cli, "decode"],
            input="\n".join(hexes) + "\n",
            capture_output=True,
            text=True,
            check=True,
        )
        answers = decoded.stdout.splitlines()
        assert len(answers) == len(values)
        for data, text in zip(hexes, answers, strict=True):
            kind, raw = text.split()
            expected = (
                int(raw) if kind == "i" else struct.unpack(">d", bytes.fromhex(raw))[0]
            )
            assert equal(
                codec.decode(bytes.fromhex(data), int if kind == "i" else float),
                expected,
            )
        total += len(values)
    for invalid_value in (True, False, "1", None):
        try:
            codec.encode(invalid_value)
        except TypeError:
            pass
        else:
            raise AssertionError(invalid_value)
    for value in (-(1 << 63) - 1, 1 << 64):
        try:
            codec.encode(value)
        except ValueError:
            pass
        else:
            raise AssertionError(value)
    print(
        f"vectors={count}, rejects={rejects}, seeded_random={total}: encode/decode identity passed"
    )


if __name__ == "__main__":
    main()
