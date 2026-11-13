# layout.py DIR [STALL_JSON]: the docids kind of every strata segment (seg-*.sx) under DIR, per shard directory:
# segments, docs and 128-ordinal blocks per kind (contiguous, monotone, permuted: strata docids.hh), span/docs.
# With stall_load's JSON (--seg-snapshots), only the loaded phase's end listing, and per shard the segments at the
# phase's start and end, merged away and new. Reads the footer (toc offset u64, toc length u64, toc crc u32, doc
# count u32, min u64, max u64) and the table of contents (strata format.hh).
import collections, json, os, struct, sys

KINDS = {0: "contiguous", 1: "monotone", 2: "permuted"}
DOCIDS, BLOCK = 7, 128


def segment(path):
    try:
        with open(path, "rb") as f:
            b = f.read()
    except OSError:
        return None
    if len(b) < 36 + 56:
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


def fmt(kinds):
    return "; ".join(
        f"{k} {v[0]} segs {v[1]} docs {v[2]} blocks span/docs {v[3] / max(v[1], 1):.1f}" for k, v in sorted(kinds.items())
    )


root = sys.argv[1]
only, segs = None, None
if len(sys.argv) > 2:
    d = json.load(open(sys.argv[2]))
    segs = d["phases"][-1].get("segs")
    if segs and segs.get("end") is not None:
        only = set(segs["end"])
per = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0, 0, 0]))   # shard -> kind -> segs, docs, blocks, span
missing = 0
for dpath, _, files in os.walk(root):
    for fn in files:
        if not (fn.startswith("seg-") and fn.endswith(".sx")):
            continue
        rel = os.path.relpath(os.path.join(dpath, fn), root)
        if only is not None and rel not in only:
            continue
        s = segment(os.path.join(dpath, fn))
        if s is None:
            continue
        kind, count, lo, hi = s
        acc = per[os.path.dirname(rel)][kind]
        acc[0] += 1
        acc[1] += count
        acc[2] += 0 if kind == "contiguous" else (count + BLOCK - 1) // BLOCK
        acc[3] += hi - lo + 1 if count else 0
if only is not None:
    missing = sum(1 for rel in only if not os.path.exists(os.path.join(root, rel)))
tot = collections.defaultdict(lambda: [0, 0, 0, 0])
for shard in sorted(per):
    line = f"  {shard}: {fmt(per[shard])}"
    if segs and segs.get("start") is not None:
        st = {r for r in segs["start"] if os.path.dirname(r) == shard}
        en = {r for r in segs["end"] if os.path.dirname(r) == shard}
        line += f" | loaded phase: {len(st)} -> {len(en)} segs, {len(st - en)} merged away, {len(en - st)} new"
    print(line)
    for k, v in per[shard].items():
        for i in range(4):
            tot[k][i] += v[i]
print(f"  total: {fmt(tot)}" + (f" ({missing} end-listed segments gone by the read)" if missing else ""))
