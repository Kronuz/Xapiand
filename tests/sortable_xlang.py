"""The Python client's codec equals Kronuz/sortable's C++ codec byte for byte (design T3).

Usage: python3 -I tests/sortable_xlang.py SORTABLE_SOURCE_DIR SORTABLE_CLI [SEED]

Checks the pinned fixtures' sha256, then: every line of test/vectors.txt through the client; 10^6
seeded values (half integers with log-uniform magnitudes, half binary64 bit patterns) through both
encoders; and decode's accept/reject and answers over every 0-2 byte string, every 3-byte string
starting with an escape tag, test/reject.txt and 10^5 seeded mutations of valid encodings.
"""

from __future__ import annotations

import hashlib
import importlib.util
import math
import random
import struct
import subprocess
import sys
from collections.abc import Iterator
from pathlib import Path
from types import ModuleType

# The pinned library's fixtures (docs/mechanisms/value-encoding.md); a pin bump that changes them fails here.
FIXTURES = {
    "vectors.txt": "c4d4a425c09e6d3ec49995f31e79a6bde62cc258cae6a1bbd4e0be03042f98c8",
    "reject.txt": "a679d215ec843a0fbdd711c9d74232834c1d1af6529638e3725234f6fdeb7213",
}
MODULE = Path(__file__).resolve().parents[1] / "contrib" / "python" / "xapiand-py" / "xapiand" / "sortable.py"
ESCAPE_TAGS = (0x81, 0xFE, 0x7E, 0x01)
RANDOM_VALUES = 1_000_000
MUTATIONS = 100_000
BATCH = 100_000


def load_codec() -> ModuleType:
    spec = importlib.util.spec_from_file_location("sortable", MODULE)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


codec = load_codec()


def bits(value: float) -> str:
    return struct.pack(">d", value).hex()


def value_line(value: float) -> str:
    return f"f {bits(value)}" if isinstance(value, float) else f"i {value}"


def run(cli: str, mode: str, lines: list[str]) -> list[str]:
    done = subprocess.run(
        [cli, mode], input="".join(f"{s}\n" for s in lines), capture_output=True, text=True, check=False
    )
    if done.returncode != 0:
        sys.exit(f"sortable_cli {mode} exited {done.returncode}: {done.stderr.strip()}")
    answers = done.stdout.split("\n")[:-1]
    if len(answers) != len(lines):
        sys.exit(f"sortable_cli {mode}: {len(answers)} answers for {len(lines)} lines")
    return answers


def batches(items: list[bytes]) -> Iterator[list[bytes]]:
    for start in range(0, len(items), BATCH):
        yield items[start : start + BATCH]


def python_answer(data: bytes) -> str:
    """The client's decode in sortable_cli's output form: reject, i N or f HEX16."""
    try:
        as_float = codec.decode(data, float)
    except ValueError:
        return "reject"
    try:
        as_int = codec.decode(data, int)
    except ValueError:
        return f"f {bits(math.nan if math.isnan(as_float) else as_float)}"
    if as_float != float(as_int):
        return f"inconsistent {as_int} {as_float!r}"
    return f"i {as_int}"


def check_fixtures(root: Path) -> None:
    for name, digest in FIXTURES.items():
        actual = hashlib.sha256((root / "test" / name).read_bytes()).hexdigest()
        if actual != digest:
            sys.exit(f"{name}: sha256 {actual}, pinned {digest}")


def check_vectors(root: Path) -> list[bytes]:
    encodings: list[bytes] = []
    for line in (root / "test" / "vectors.txt").read_text().splitlines():
        kind, text, hexed = line.split()
        value: int | float = float(text) if kind == "d" else int(text)
        data = bytes.fromhex(hexed)
        if codec.encode(value) != data:
            sys.exit(f"vectors.txt: encode({text}) = {codec.encode(value).hex()}, want {hexed}")
        back = codec.decode(data, float if kind == "d" else int)
        if not (back == value or (isinstance(value, float) and math.isnan(value) and math.isnan(back))):
            sys.exit(f"vectors.txt: decode({hexed}) = {back!r}, want {text}")
        encodings.append(data)
    return encodings


def random_value(rnd: random.Random) -> int | float:
    if rnd.randrange(2):
        return struct.unpack(">d", rnd.getrandbits(64).to_bytes(8, "big"))[0]
    width = rnd.randrange(65)  # log-uniform magnitude: |v| in [2^(width-1), 2^width)
    magnitude = rnd.randrange(1 << (width - 1), 1 << width) if width else 0
    if rnd.randrange(2) and magnitude <= 1 << 63:
        return -magnitude
    return magnitude


def check_encode(cli: str, rnd: random.Random) -> list[bytes]:
    encodings: list[bytes] = []
    for _ in range(RANDOM_VALUES // BATCH):
        values = [random_value(rnd) for _ in range(BATCH)]
        for value, hexed in zip(values, run(cli, "encode", [value_line(v) for v in values]), strict=True):
            mine = codec.encode(value).hex()
            if mine != hexed:
                sys.exit(f"encode {value_line(value)}: python {mine}, C++ {hexed}")
            encodings.append(bytes.fromhex(hexed))
    return encodings


def mutate(rnd: random.Random, data: bytes) -> bytes:
    buf = bytearray(data)
    match rnd.randrange(5):
        case 0 if buf:
            buf[rnd.randrange(len(buf))] = rnd.randrange(256)
        case 1 if buf:
            del buf[rnd.randrange(len(buf)) :]
        case 2:
            buf.append(rnd.randrange(256))
        case 3:
            buf.insert(rnd.randrange(len(buf) + 1), rnd.randrange(256))
        case _ if buf:
            del buf[rnd.randrange(len(buf))]
    return bytes(buf)


def check_decode(cli: str, inputs: list[bytes]) -> int:
    accepted = 0
    for chunk in batches(inputs):
        for data, theirs in zip(chunk, run(cli, "decode", [d.hex() for d in chunk]), strict=True):
            mine = python_answer(data)
            if mine != theirs:
                sys.exit(f"decode {data.hex() or '(empty)'}: python {mine}, C++ {theirs}")
            if mine != "reject":
                accepted += 1
                if codec.encode(codec.decode(data, int if mine[0] == "i" else float)) != data:
                    sys.exit(f"decode {data.hex()}: accepted bytes do not re-encode to themselves")
    return accepted


def main() -> None:
    root, cli = Path(sys.argv[1]), sys.argv[2]
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 611
    rnd = random.Random(seed)
    check_fixtures(root)
    vectors = check_vectors(root)
    randoms = check_encode(cli, rnd)

    short = [b""] + [bytes([a]) for a in range(256)] + [bytes([a, b]) for a in range(256) for b in range(256)]
    escaped = [bytes([t, a, b]) for t in ESCAPE_TAGS for a in range(256) for b in range(256)]
    rejects = []
    for line in (root / "test" / "reject.txt").read_text().splitlines():
        hexed = line.split()[-1]
        rejects.append(b"" if hexed == "-" else bytes.fromhex(hexed))
    valid = vectors + randoms
    mutations = [mutate(rnd, valid[rnd.randrange(len(valid))]) for _ in range(MUTATIONS)]
    inputs = vectors + randoms + short + escaped + rejects + mutations
    accepted = check_decode(cli, inputs)
    if any(python_answer(data) != "reject" for data in rejects):
        sys.exit("reject.txt: the client accepts a reject witness")

    for invalid in (True, False, "1", None):
        try:
            codec.encode(invalid)
        except TypeError:
            continue
        sys.exit(f"encode({invalid!r}) did not raise TypeError")
    for out_of_range in (-(1 << 63) - 1, 1 << 64):
        try:
            codec.encode(out_of_range)
        except ValueError:
            continue
        sys.exit(f"encode({out_of_range}) did not raise ValueError")

    print(
        f"seed {seed}: {len(vectors)} vectors, {len(randoms)} random encodes, {len(inputs)} decodes "
        f"({accepted} accepted, {len(rejects)} reject witnesses, {MUTATIONS} mutations): C++ and Python agree"
    )


if __name__ == "__main__":
    main()
