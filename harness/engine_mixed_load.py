#!/usr/bin/env python3
"""
engine_mixed_load.py -- concurrent index and search on one single-shard index, for T10 (run under a sanitizer build).

  engine_mixed_load.py BIN OUTDIR [--engine strata] [--secs 600] [--writers 3] [--searchers 4] [--port 18990]

Starts one node, creates /mix/ with one shard, then for --secs seconds
  writers    PUT single documents, RESTORE batches of 50, replace a known id, DELETE a known id; every acknowledged write
             updates a model {id: accountNumber}; ids are disjoint per writer
  searchers  SEARCH /mix/ with the loadtest query mix plus a text query, a sorted query and an aggregation
Afterwards it commits, stops the node, and checks: no 5xx on a write, no 5xx on a search, the final DUMP equals the model.
The node's stderr/stdout are in OUTDIR/node.log; sanitizer reports go to OUTDIR/san.* (TSAN_OPTIONS/ASAN_OPTIONS log_path).
Exit 0 only when the checks pass and the sanitizer wrote no report.
"""
import argparse, collections, http.client, json, os, random, subprocess, sys, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from loadtest import QUERY_MIX  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('bin'); ap.add_argument('out')
ap.add_argument('--engine', default='strata')
ap.add_argument('--secs', type=int, default=600)
ap.add_argument('--writers', type=int, default=3)
ap.add_argument('--searchers', type=int, default=4)
ap.add_argument('--port', type=int, default=18990)
ap.add_argument('--seed', type=int, default=1)
a = ap.parse_args()
a.bin = os.path.abspath(a.bin); a.out = os.path.abspath(a.out)
os.makedirs(a.out, exist_ok=True)
data = os.path.join(a.out, 'data')
subprocess.run(['rm', '-rf', data]); os.makedirs(data)
for f in os.listdir(a.out):
    if f.startswith('san.'): os.unlink(os.path.join(a.out, f))
rows = [json.loads(l) for l in open(os.path.join(HERE, '..', 'docs/public/assets/accounts.ndjson')) if l.strip()]

env = dict(os.environ)
san = os.path.join(a.out, 'san')
env.setdefault('TSAN_OPTIONS', f'log_path={san} second_deadlock_stack=1 history_size=4')
env.setdefault('ASAN_OPTIONS', f'log_path={san} detect_leaks=0')
env.setdefault('UBSAN_OPTIONS', f'log_path={san} print_stacktrace=1')
log = open(os.path.join(a.out, 'node.log'), 'wb')
node = subprocess.Popen([a.bin, '--port', str(a.port), '--name', 'mix', '-D', data], stdout=log, stderr=log, env=env)


def conn(): return http.client.HTTPConnection('localhost', a.port, timeout=120)


def call(c, method, path, body=None, ct='application/json'):
    c.request(method, path, body=body, headers={'Content-Type': ct})
    r = c.getresponse(); b = r.read()
    return r.status, b


for _ in range(480):
    try:
        c = conn(); call(c, 'GET', '/'); break
    except Exception: time.sleep(.5)
else:
    node.kill(); sys.exit('node did not come up')
st, b = call(conn(), 'PUT', '/mix/', json.dumps({'_settings': {'number_of_shards': 1}}))
assert st < 300, (st, b[:200])

model = {}; mlock = threading.Lock()
stats = collections.Counter(); slock = threading.Lock()
fails = []
stop = threading.Event()


def count(k):
    with slock: stats[k] += 1


def doc(rng, i):
    d = dict(rng.choice(rows)); d['_id'] = i
    return d


def writer(w):
    rng = random.Random(a.seed * 100 + w); c = conn(); n = 0
    base = (w + 1) * 10_000_000
    while not stop.is_set():
        op = rng.random(); n += 1
        try:
            if op < 0.45:
                i = base + n; d = doc(rng, i)
                st, b = call(c, 'PUT', f'/mix/{i}', json.dumps(d))
                what = 'put'; ok = st < 300
                if ok:
                    with mlock: model[i] = d['accountNumber']
            elif op < 0.7:
                ids = [base + n * 100 + k for k in range(50)]; ds = [doc(rng, i) for i in ids]
                st, b = call(c, 'RESTORE', '/mix/?commit' if rng.random() < .3 else '/mix/', '\n'.join(json.dumps(d) for d in ds) + '\n', 'application/x-ndjson')
                what = 'restore'; ok = st < 300
                if ok:
                    with mlock: model.update({i: d['accountNumber'] for i, d in zip(ids, ds)})
            elif op < 0.85:
                with mlock: known = list(model)[-200:] if model else []
                known = [i for i in known if base < i < base + 10_000_000]
                if not known: continue
                i = rng.choice(known); d = doc(rng, i)
                st, b = call(c, 'PUT', f'/mix/{i}', json.dumps(d))
                what = 'replace'; ok = st < 300
                if ok:
                    with mlock: model[i] = d['accountNumber']
            else:
                with mlock: known = [i for i in list(model)[-200:] if base < i < base + 10_000_000]
                if not known: continue
                i = rng.choice(known)
                st, b = call(c, 'DELETE', f'/mix/{i}')
                what = 'delete'; ok = st < 300
                if ok:
                    with mlock: model.pop(i, None)
            count(f'write.{what}.{"ok" if ok else st}')
            if not ok: fails.append((what, st, b[:200]))
        except Exception as e:
            fails.append(('write-exception', repr(e)))
            c = conn()


SEARCHES = [(n, json.dumps(q)) for n, q in QUERY_MIX] + [
    ('text', json.dumps({'_query': {'personality': 'elegant'}})),
    ('sorted', json.dumps({'_query': {'gender': 'male'}, '_sort': {'age': 'asc'}, '_limit': 20})),
    ('agg', json.dumps({'_query': {'gender': 'female'}, '_limit': 0, '_aggs': {'by_state': {'_values': {'_field': 'contact.state'}}}})),
]


def searcher(s):
    rng = random.Random(a.seed * 1000 + s); c = conn()
    while not stop.is_set():
        name, body = rng.choice(SEARCHES)
        try:
            st, b = call(c, 'SEARCH', '/mix/', body)
            count(f'search.{name}.{st if st >= 400 else "ok"}')
            if st >= 500: fails.append(('search', name, st, b[:200]))
        except Exception as e:
            fails.append(('search-exception', name, repr(e)))
            c = conn()


threads = [threading.Thread(target=writer, args=(w,)) for w in range(a.writers)] + \
          [threading.Thread(target=searcher, args=(s,)) for s in range(a.searchers)]
t0 = time.time()
for t in threads: t.start()
while time.time() - t0 < a.secs and node.poll() is None: time.sleep(1)
stop.set()
for t in threads: t.join(timeout=300)
died = node.poll() is not None
if not died:
    call(conn(), 'POST', '/mix/:commit')
    st, b = call(conn(), 'DUMP', '/mix/')
    final = {int(r['_id']): r['accountNumber'] for r in json.loads(b)} if st == 200 else None
    node.terminate()
    try: node.wait(timeout=120)
    except subprocess.TimeoutExpired: node.kill()
else:
    final = None
reports = [f for f in os.listdir(a.out) if f.startswith('san.')]
res = {'secs': round(time.time() - t0), 'engine': a.engine, 'writers': a.writers, 'searchers': a.searchers, 'node_died': died,
       'node_rc': node.returncode, 'stats': dict(sorted(stats.items())), 'model_docs': len(model),
       'final_docs': None if final is None else len(final), 'model_equals_dump': final == model if final is not None else None,
       'failures': len(fails), 'first_failures': [list(map(str, f)) for f in fails[:5]], 'sanitizer_reports': reports}
json.dump(res, open(os.path.join(a.out, 'result.json'), 'w'), indent=1)
print(json.dumps(res, indent=1))
sys.exit(0 if not died and not fails and res['model_equals_dump'] and not reports else 1)
