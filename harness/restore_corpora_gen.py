# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN

"""restore_corpora_gen.py OUT [N]: writes the source documents of the restore corpora, as NDJSON, deterministically.

N documents (default 3000) with every value shape RESTORE must round-trip: text in several scripts, integers (negative and
past 2^31), floats, booleans, dates, geo points, arrays, nested objects, empty strings and arrays, and long texts. The same
file is posted to each build that writes a corpus, and is what a replay compares every restored document with.
"""

import json
import random
import sys

WORDS = [
    "alpha",
    "beta",
    "gamma",
    "delta",
    "index",
    "search",
    "shard",
    "segment",
    "commit",
    "merge",
    "restore",
    "dump",
    "stream",
    "ticket",
    "token",
    "cluster",
    "node",
    "query",
    "filter",
    "range",
    "bucket",
    "schema",
    "field",
    "text",
    "keyword",
    "stored",
    "value",
    "paper",
    "river",
    "stone",
    "cloud",
    "engine",
    "reader",
    "writer",
]
OTHER = [
    "café",
    "naïve",
    "日本語のテキスト",
    "Привет мир",
    "مرحبا بالعالم",
    "emoji 🚀 ok",
    "tab\tand\nnewline",
    'quote " and \\ slash',
]


def document(rnd: random.Random, i: int) -> dict[str, object]:
    doc: dict[str, object] = {
        "_id": i + 1,
        "user": f"user{rnd.randrange(97)}",
        "message": " ".join(rnd.choice(WORDS) for _ in range(rnd.randrange(3, 24))),
        "count": rnd.randrange(-1000, 1_000_000),
        "big": rnd.randrange(-(2**40), 2**40),
        "score": round(rnd.uniform(-500, 500), 4),
        "flag": rnd.random() < 0.5,
        "when": f"{rnd.randrange(1990, 2027):04d}-{rnd.randrange(1, 13):02d}-{rnd.randrange(1, 29):02d}T{rnd.randrange(24):02d}:{rnd.randrange(60):02d}:00Z",
        "tags": [rnd.choice(WORDS) for _ in range(rnd.randrange(0, 5))],
        "author": {
            "name": f"n{rnd.randrange(50)}",
            "age": rnd.randrange(18, 90),
            "address": {"city": rnd.choice(WORDS), "zip": f"{rnd.randrange(100000):05d}"},
        },
    }
    if i % 7 == 0:
        doc["other"] = rnd.choice(OTHER)
    if i % 11 == 0:
        doc["note"] = ""
    if i % 13 == 0:
        doc["long"] = " ".join(rnd.choice(WORDS) for _ in range(400))
    if i % 17 == 0:
        doc["where"] = {
            "_point": {"_latitude": round(rnd.uniform(-80, 80), 5), "_longitude": round(rnd.uniform(-170, 170), 5)}
        }
    return doc


def main() -> None:
    out = sys.argv[1]
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 3000
    rnd = random.Random(0x7C0)
    with open(out, "w", encoding="utf-8") as f:
        f.writelines(json.dumps(document(rnd, i), ensure_ascii=False) + "\n" for i in range(n))


if __name__ == "__main__":
    main()
