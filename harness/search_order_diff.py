#!/usr/bin/env python3
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
# THE SOFTWARE.

"""search_order_diff.py A.json B.json -- what bodydiff.py's bag comparison hides: the order of `hits` and their scores.

For every request present in both newman reports whose body has a `hits` list, compare the ordered ids, then the
scores (relative difference above 1e-6 counts) and `total`/`count`. Prints one line per difference and a summary."""
import json, sys
from collections import defaultdict


def load(path):
    out, seen = defaultdict(list), defaultdict(int)
    for e in json.load(open(path))['run']['executions']:
        req = e['request']; u = req['url']
        key = (req['method'], '/' + '/'.join(u.get('path') or []), (e['item'] or {}).get('name'))
        raw = bytes(((e.get('response') or {}).get('stream') or {}).get('data') or []).decode('utf-8', 'replace')
        try:
            out[key].append(json.loads(raw))
        except Exception:
            out[key].append(None)
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
n = same = 0
for key in a:
    for i, (x, y) in enumerate(zip(a[key], b.get(key, []))):
        if not (isinstance(x, dict) and isinstance(y, dict) and isinstance(x.get('hits'), list) and isinstance(y.get('hits'), list)):
            continue
        n += 1
        hid = lambda h: h.get('_id') if isinstance(h, dict) else h
        ix = [hid(h) for h in x['hits']]; iy = [hid(h) for h in y['hits']]
        notes = []
        if ix != iy:
            notes.append('order/ids differ' + (' (same set)' if sorted(map(json.dumps, ix)) == sorted(map(json.dumps, iy)) else ' (different set)'))
        sc = lambda h: h.get('_score') if isinstance(h, dict) else None
        sx = [sc(h) for h in x['hits']]; sy = [sc(h) for h in y['hits']]
        if ix == iy and any(p is not None and q is not None and abs(p - q) > 1e-6 * max(1, abs(p)) for p, q in zip(sx, sy)):
            notes.append('scores differ: %s vs %s' % (sx[:3], sy[:3]))
        for f in ('total', 'count'):
            if x.get(f) != y.get(f):
                notes.append(f'{f} {x.get(f)} vs {y.get(f)}')
        if notes:
            print(f'{key[0]} {key[1]} [{key[2]}] #{i}: ' + '; '.join(notes))
        else:
            same += 1
print(f'{n} responses with hits compared; {same} identical in order, scores, total and count')
