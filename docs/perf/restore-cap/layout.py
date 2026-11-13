# layout.py DIR: the docids kind of every strata segment (seg-*.sx) under DIR, per shard directory: contiguous,
# monotone or permuted (strata docids.hh), with docs and span (max - min + 1) / docs. Reads the footer and table of
# contents (strata format.hh): footer = toc offset u64, toc length u64, toc crc u32, doc count u32, min u64, max u64.
import collections, os, struct, sys

KINDS = {0: "contiguous", 1: "monotone", 2: "permuted"}
DOCIDS = 7


def segment(path):
    with open(path, "rb") as f:
        b = f.read()
    if len(b) < 36 + 56 or b[-4:] == b"":
        return None
    toc_off, toc_len, _crc, count, lo, hi = struct.unpack_from("<QQIIQQ", b, len(b) - 56)
    t = b[toc_off : toc_off + toc_len]
    (n,) = struct.unpack_from("<I", t, 0)
    p = 4
    for _ in range(n):
        kind, shift, off, length, _c = struct.unpack_from("<HHQQI", t, p)
        p += 24 + 4 * ((length + (1 << shift) - 1) >> shift)
        if kind == DOCIDS:
            return KINDS.get(b[off], f"kind {b[off]}"), count, lo, hi
    return None


root = sys.argv[1]
per = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0, 0]))   # shard -> kind -> segs, docs, span
for d, _, files in os.walk(root):
    for fn in files:
        if fn.startswith("seg-") and fn.endswith(".sx"):
            s = segment(os.path.join(d, fn))
            if s is None:
                continue
            kind, count, lo, hi = s
            shard = os.path.relpath(d, root)
            acc = per[shard][kind]
            acc[0] += 1
            acc[1] += count
            acc[2] += hi - lo + 1 if count else 0
tot = collections.defaultdict(lambda: [0, 0, 0])
for shard in sorted(per):
    print(f"  {shard}: " + "; ".join(f"{k} {v[0]} segs {v[1]} docs span/docs {v[2] / max(v[1], 1):.2f}" for k, v in sorted(per[shard].items())))
    for k, v in per[shard].items():
        for i in range(3):
            tot[k][i] += v[i]
print("  total: " + "; ".join(f"{k} {v[0]} segs {v[1]} docs span/docs {v[2] / max(v[1], 1):.2f}" for k, v in sorted(tot.items())))
