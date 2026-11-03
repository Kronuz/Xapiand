#!/usr/bin/env bash
# profile_bulk.sh BIN OUTDIR [--mult N] [--schema JSON] [--label NAME] [--port P]
#
# Rerunnable per-layer profile of a bulk RESTORE (`sample` on macOS, `perf` on Linux via perf_sample.py). BIN is any xapiand binary
# (RelWithDebInfo with -fno-omit-frame-pointer gives the best symbols). For each run it
#   1. smoke: fresh node, one timed N x 1000-doc ndjson RESTORE (default N=20 -> 20000 docs),
#      docs/s plus server user/sys CPU;
#   2. profile: fresh node, same RESTORE sampled end to end with sample(1) or perf, attributed by
#      thread group and by layer (rules in LAYERS below; edit them when frames change).
# Writes OUTDIR/<label>.json, <label>.md and the raw <label>.sample.txt.
# Run it under a hold: quiet strict NAME MIN "what" -- harness/profile_bulk.sh BIN OUT
set -eu
[ $# -ge 2 ] || { sed -n 2,12p "$0"; exit 2; }
. "$(dirname "$0")/env.sh"
BIN=$1; OUT=$2; shift 2
HARNESS_DIR=$(cd "$(dirname "$0")" && pwd) exec "$PY" -I - "$BIN" "$OUT" "$@" <<'PY'
import sys, os, re, json, time, shutil, subprocess, http.client, resource, collections, argparse, signal
ap = argparse.ArgumentParser()
ap.add_argument('bin'); ap.add_argument('out')
ap.add_argument('--mult', type=int, default=20)
ap.add_argument('--schema', default=None)
ap.add_argument('--label', default='bulk')
ap.add_argument('--port', type=int, default=18990)
ap.add_argument('--dataset', default=None)
ap.add_argument('--sample-secs', type=int, default=0)
ap.add_argument('--reanalyze', action='store_true', help='skip the runs; redo the analysis from OUT/<label>.sample.txt and .json')
ap.add_argument('--min-sample-frac', type=float, default=0.25, help='invalid below this fraction of the sampling window at 1 ms (a healthy run is about 0.55)')
ap.add_argument('--min-sibling-frac', type=float, default=0.5, help='invalid below this fraction of the median wall samples of the other profiles in OUTDIR')
a = ap.parse_args(); MIN_FRAC = a.min_sample_frac; MIN_FRAC2 = a.min_sibling_frac; a.bin = os.path.abspath(a.bin); a.out = os.path.abspath(a.out)
repo = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip()
ds = a.dataset or os.path.join(repo, 'docs/public/assets/accounts.ndjson')
lines = [l for l in open(ds, 'rb').read().split(b'\n') if l.strip()]
body = b'\n'.join(lines * a.mult) + b'\n'; ndocs = len(lines) * a.mult
os.makedirs(a.out, exist_ok=True)
data = os.path.join(a.out, f'{a.label}.data')

def start():
    shutil.rmtree(data, ignore_errors=True); os.makedirs(data)
    p = subprocess.Popen([a.bin, '--port', str(a.port), '--name', 'benchnode', '-D', data],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(240):
        try:
            c = http.client.HTTPConnection('localhost', a.port, timeout=5); c.request('GET', '/'); r = c.getresponse(); r.read()
            return p
        except Exception: time.sleep(.5)
    p.kill(); raise SystemExit('server did not come up')

def restore(p, sampler=None):
    if a.schema:
        c = http.client.HTTPConnection('localhost', a.port, timeout=60)
        c.request('PUT', '/bench/', body=a.schema.encode(), headers={'Content-Type': 'application/json'}); r = c.getresponse(); r.read()
        assert r.status < 300, ('schema PUT', r.status)
    c = http.client.HTTPConnection('localhost', a.port, timeout=600)
    if sampler: sampler()
    t = time.perf_counter()
    c.request('RESTORE', '/bench/?commit', body=body, headers={'Content-Type': 'application/x-ndjson'})
    r = c.getresponse(); r.read(); e = time.perf_counter() - t
    assert r.status in (200, 201, 204), r.status
    return e

def stop(p):
    p.terminate()
    _, st, ru = os.wait4(p.pid, 0)
    return ru.ru_utime, ru.ru_stime

rawp = os.path.join(a.out, f'{a.label}.sample.txt')
if a.reanalyze:
    res = json.load(open(os.path.join(a.out, f'{a.label}.json'))); e2 = res['profile_wall_s']; shards = 0
else:
    res = {'label': a.label, 'docs': ndocs, 'bin': a.bin, 'schema': a.schema}
    # 1. smoke
    p = start(); e = restore(p); u, s = stop(p)
    res['smoke'] = {'wall_s': e, 'docs_per_s': ndocs / e, 'user_s': u, 'sys_s': s, 'cpu_ms_per_doc': (u + s) * 1000 / ndocs}
    # 2. profile
    p = start(); sm = []
    secs = a.sample_secs or max(8, int(ndocs / 3000) + 4)
    sampler = [os.path.join(os.environ['HARNESS_DIR'], 'perf_sample.py'), str(p.pid), str(secs), rawp] if sys.platform.startswith('linux') else ['sample', str(p.pid), str(secs), '-file', rawp]
    def go(): sm.append(subprocess.Popen(sampler, stdout=subprocess.DEVNULL)); time.sleep(0.4)
    e2 = restore(p, go)
    sm[0].wait()
    shards = subprocess.run(['find', data, '-maxdepth', '3', '-type', 'd'], capture_output=True, text=True).stdout.count('\n')
    stop(p); shutil.rmtree(data, ignore_errors=True)
    res['profile_wall_s'] = e2


# ---- analysis ----
R = re.compile(r'^([ +!:|]*?)(\d+) (.*)$')
def clean(n):
    n = re.sub(r'\s+\(in ([^)]*)\).*$', lambda m: ' @' + m.group(1) if 'xapiand' not in m.group(1) else '', n)
    n = re.sub(r'\s+\+\s+[\d,.]+\s+\[0x.*$', '', n)
    for _ in range(4): n = re.sub(r'<[^<>]*>', '', n)
    return n[:110]
nodes = []; stack = []; inside = False; total_samples = 0
for ln in open(rawp, errors='replace'):
    if ln.startswith('Call graph:'): inside = True; continue
    if not inside: continue
    if ln.startswith('Total number in stack') or ln.startswith('Sort by top'): break
    m = R.match(ln.rstrip('\n'))
    if not m: continue
    d = len(m.group(1)); c = int(m.group(2))
    while stack and stack[-1][0] >= d: stack.pop()
    node = [d, c, m.group(3), 0, [x[2] for x in stack]]
    if stack: stack[-1][3] += c
    stack.append(node); nodes.append(node)
    if not node[4]: total_samples = max(total_samples, c)
rows = []  # (thread, frames root..leaf cleaned, self)
for d, c, name, cs, anc in nodes:
    if c - cs > 0 and anc: rows.append((anc[0], [clean(x) for x in anc[1:]] + [clean(name)], c - cs))
def tgroup(th):
    m = re.search(r'Thread_\d+\s*:?\s*(.*)$', th); n = m.group(1).strip() if m else th
    return 'main' if 'main-thread' in n else (re.sub(r'\d+$', '', n) or 'unnamed')
IDLE = re.compile(r'psynch_cvwait|^kevent|mach_msg|semwait|__select|nanosleep|semaphore_wait|^poll|__sigwait|futex_wait|epoll_wait')
# A thread blocked in a device flush (F_FULLFSYNC and F_BARRIERFSYNC are __fcntl, plus fsync) burns no CPU: its own line.
FLUSH = re.compile(r'^(__fcntl|fsync|fdatasync|__fsync_nocancel|fsync_nocancel)\b')
LOCK = re.compile(r'psynch_mutexwait|ulock_wait|psynch_rw')

# layer -> (replaced?, regex over a frame); first frame matching, scanning leaf to root, wins
LAYERS = [
 ('WAL (delete/write/replay)',            'replaced 7b', r'DatabaseWAL|delete_wal|delete_files|\bwal\.cc|WALWriter'),
 ('Xapian::Document in-memory build',     'replaced S1', r'Xapian::Document::Internal|Xapian::TermIterator|Xapian::Document::'),
 ('Xapian engine (add/replace/commit)',   'replaced S1', r'Glass|Xapian::|Inverter|DocumentTermList|CompressionStream|engine::xapian::|io_read_block|io_write_block|Honey|Brass'),
 ('Strata engine (add/replace/commit)',  'replaced S1', r'engine::strata::|\bstrata::|\bprospect::'),
 ('Shard checkout / sync_wait / lock',    'replaced 7c', r'ShardEndpoint|DatabasePool|lock_shard|sync_wait|packaged_task|DatabaseHandler::get_docid_term|Shard::find_key|Shard::replace_document'),
 ('Term generation (analysis, snowball)', 'stays',       r'GenerateTerms|analysis::|snowball|term_generator|Schema::index_item\b'),
 ('Schema processing',                    'stays',       r'Schema::'),
 ('Data record build (zstd, locator)',    'stays',       r'Data::|Locator|compress_zstd|ZstdCompress|ZSTD|storage|Storage|volume|HUF_|FSE_|finalise_write'),
 ('JSON/MsgPack decode',                  'stays',       r'rapidjson|msgpack::|Json|json|ndjson|NDJSON|ObjectBody|Parser|parser|yyjson'),
 ('HTTP/reactor/request parsing',         'stays',       r'restore_database_view|Http|http|asio|reactor|Reactor|kqueue|ClientSession|Request|Response|nghttp|llhttp'),
 ('Doc pipeline glue (prepare/index)',    'stays',       r'DocIndexer|DatabaseHandler|engine::Document'),
]
LAYERS = [(n, r, re.compile(p)) for n, r, p in LAYERS]
DATA_RX = re.compile(r'Data::|Locator|finalise_write')
def classify(frames, flush=True):
    leaf = frames[-1]
    if IDLE.search(leaf):
        return 'LOCKWAIT:shard writable checkout' if any('_writable_checkout' in f or 'ShardEndpoint' in f for f in frames) else 'IDLE'
    if flush and FLUSH.search(leaf): return 'FLUSHWAIT'
    for f in reversed(frames):
        for n, r, rx in LAYERS:
            if rx.search(f):
                if n.startswith(('JSON', 'Doc pipeline')) and any(DATA_RX.search(x) for x in frames): return [n for n, r, rx in LAYERS if n.startswith('Data record')][0]
                return n
    return 'other/unattributed (malloc, libc, runtime)'
cnt = collections.Counter(); tfun = collections.defaultdict(collections.Counter)
tg = collections.defaultdict(lambda: collections.Counter())
flush_by = collections.Counter()
perthread = collections.defaultdict(collections.Counter)
for th, fr, s in rows:
    cl = classify(fr); g = tgroup(th)
    if cl == 'IDLE': tg[g]['idle'] += s; continue
    if cl.startswith('LOCKWAIT'): tg[g]['lockwait'] += s; perthread[th]['lockwait'] += s; cnt[cl] += s; continue
    if cl == 'FLUSHWAIT':
        tg[g]['flushwait'] += s; perthread[th]['flushwait'] += s; cnt[cl] += s; fl = classify(fr, flush=False); flush_by[fl] += s; continue
    if LOCK.search(fr[-1]): tg[g]['lock'] += s
    tg[g]['busy'] += s; perthread[th]['busy'] += s
    cnt[cl] += s
    # attribute the function: first frame of the winning layer going up
    for f in reversed(fr):
        if cl in [n for n, r, rx in LAYERS] and dict((n, rx) for n, r, rx in LAYERS)[cl].search(f): tfun[cl][f] += s; break
    else: tfun[cl][fr[-1]] += s
busy_total = sum(v for k, v in cnt.items() if not k.startswith(('LOCKWAIT', 'FLUSHWAIT')))
# busiest shard-writer thread: busy + lock wait = its critical-path timeline
dis = {th: v for th, v in perthread.items() if 'DI' in th}
worst = max(dis.items(), key=lambda kv: kv[1]['busy'] + kv[1]['lockwait'] + kv[1]['flushwait']) if dis else None
layer_rows = []
for n, rep, _ in LAYERS + [('other/unattributed (malloc, libc, runtime)', 'stays', None)]:
    c = cnt.get(n, 0)
    layer_rows.append({'layer': n, 'status': rep, 'samples': c, 'cpu_pct': 100.0 * c / busy_total if busy_total else 0,
                       'cores': c / total_samples if total_samples else 0,
                       'top_functions': [[f, v] for f, v in tfun[n].most_common(3)]})
# critical path: layer split of the busiest shard-writer thread
crit = collections.Counter(); crit_flush = 0
if worst:
    for th, fr, s in rows:
        if th != worst[0]: continue
        cl = classify(fr, flush=False)  # the critical thread's timeline keeps its flush wait
        if cl == 'IDLE': continue
        crit[cl] += s
        if FLUSH.search(fr[-1]): crit_flush += s
ctot = sum(crit.values())
for r in layer_rows: r['critical_pct'] = 100.0 * crit.get(r['layer'], 0) / ctot if ctot else 0
res['critical_lockwait_pct'] = 100.0 * crit.get('LOCKWAIT:shard writable checkout', 0) / ctot if ctot else 0
res['profile'] = {'wall_samples': total_samples, 'busy_samples': busy_total, 'busy_cores': busy_total / total_samples if total_samples else 0,
                  'layers': layer_rows, 'thread_groups': {g: dict(c) for g, c in tg.items()},
                  'critical_thread': worst[0] if worst else None, 'dir_count_in_data': shards}
rep_pct = sum(r['cpu_pct'] for r in layer_rows if r['status'].startswith('replaced'))
res['profile']['replaced_cpu_pct'] = rep_pct
flush_total = cnt.get('FLUSHWAIT', 0)
flush_engine = sum(v for k, v in flush_by.items() if k in [n for n, r, rx in LAYERS if r == 'replaced S1'])
res['profile']['flush_wait'] = {'samples': flush_total, 'ms_per_doc': flush_total / ndocs, 'engine_rows_samples': flush_engine,
    'engine_rows_ms_per_doc': flush_engine / ndocs, 'by_layer': dict(flush_by), 'note': 'blocked in a device flush; not in busy samples or any CPU share'}
# The engine share (rows 'replaced S1'): CPU and critical-path time per document. A sample is 1 ms of one thread (sample(1) default).
s1 = [r for r in layer_rows if r['status'] == 'replaced S1']
engine_cpu_ms_doc = sum(r['samples'] for r in s1) / ndocs
engine_crit_ms_doc = sum(crit.get(r['layer'], 0) for r in s1) / ndocs
res['profile']['engine_share'] = {'cpu_ms_per_doc': engine_cpu_ms_doc, 'critical_thread_ms_per_doc': engine_crit_ms_doc,
    'cpu_pct': sum(r['cpu_pct'] for r in s1), 'critical_pct': sum(r['critical_pct'] for r in s1),
    'cpu_ms_per_doc_by_smoke': res['smoke']['cpu_ms_per_doc'] * sum(r['cpu_pct'] for r in s1) / 100}
sm_ = res['smoke']
if sys.platform == 'darwin':
    ncore = int(subprocess.check_output(['sysctl', '-n', 'hw.physicalcpu']).strip())
else:  # distinct (core, socket) pairs of lscpu; the logical count when lscpu is missing
    try: ncore = len({l for l in subprocess.check_output(['lscpu', '-p=CORE,SOCKET'], text=True).splitlines() if not l.startswith('#')})
    except (OSError, subprocess.CalledProcessError): ncore = os.cpu_count() or 1
cpu = sm_['cpu_ms_per_doc']
def pct(pred): return sum(r['cpu_pct'] for r in layer_rows if pred(r))
rest = cpu * (100 - rep_pct) / 100
front = cpu * pct(lambda r: r['layer'].startswith(('HTTP', 'JSON'))) / 100
res['amdahl'] = {
  'physical_cores': ncore, 'baseline_docs_per_s': sm_['docs_per_s'], 'cpu_ms_per_doc': cpu,
  'remaining_cpu_ms_per_doc_without_replaced_layers': rest,
  'docs_per_s_cpu_bound_without_replaced_layers': ncore * 1000 / rest if rest else None,
  'docs_per_s_cpu_bound_without_xapian_only': ncore * 1000 / (cpu * (100 - pct(lambda r: r['status'] == 'replaced S1')) / 100),
  'single_thread_front_end_ms_per_doc': front, 'docs_per_s_front_end_one_thread': 1000 / front if front else None,
  'note': 'CPU-bound estimates: cores x 1000 / remaining CPU ms per doc, assuming the remaining stages scale across cores'}
# A profile whose wall samples fall far short is truncated (sample(1) caught a fraction of the load): flag it invalid.
res['sample_secs'] = res.get('sample_secs') or (a.sample_secs or max(8, int(ndocs / 3000) + 4))
sib = sorted(j['profile']['wall_samples'] for f in os.listdir(a.out) if f.endswith('.json') and f != f'{a.label}.json'
             for j in [json.load(open(os.path.join(a.out, f)))] if isinstance(j, dict) and j.get('profile', {}).get('wall_samples') and not j.get('invalid') and j.get('docs') == ndocs)
invalid = []
if total_samples < MIN_FRAC * res['sample_secs'] * 1000:
    invalid.append(f"{total_samples} wall samples is under {MIN_FRAC:.0%} of the {res['sample_secs']} s window at 1 ms")
if len(sib) >= 2 and total_samples < MIN_FRAC2 * sib[len(sib) // 2]:
    invalid.append(f"{total_samples} wall samples is under {MIN_FRAC2:.0%} of its siblings' median {sib[len(sib) // 2]} ({len(sib)} siblings)")
res['invalid'] = '; '.join(invalid) or None
json.dump(res, open(os.path.join(a.out, f'{a.label}.json'), 'w'), indent=1)
layer_rows.append({'layer': 'Device-flush wait (blocked, not CPU)', 'status': 'blocked', 'samples': flush_total, 'cpu_pct': 0.0, 'cores': 0, 'critical_pct': 100.0 * crit_flush / ctot if ctot else 0, 'top_functions': []})
layer_rows.append({'layer': 'Shard writable-checkout wait (blocked, not CPU)', 'status': 'replaced 7c', 'samples': cnt.get('LOCKWAIT:shard writable checkout', 0), 'cpu_pct': 0.0, 'cores': 0, 'critical_pct': res['critical_lockwait_pct'], 'top_functions': []})
md = [f"# {a.label}", '', f"{ndocs} docs, binary `{a.bin}`", '',
      f"smoke: {sm_['docs_per_s']:.0f} docs/s, wall {sm_['wall_s']:.2f} s, user {sm_['user_s']:.2f} s, sys {sm_['sys_s']:.2f} s ({sm_['cpu_ms_per_doc']:.3f} ms CPU/doc)", '',
      *([f"INVALID PROFILE: {res['invalid']}", ''] if res['invalid'] else []),
      f"profile: {res['profile']['busy_cores']:.2f} busy cores over {total_samples} samples; device-flush wait {flush_total} samples ({flush_total / ndocs:.4f} ms/doc, {flush_engine / ndocs:.4f} in the engine rows), kept out of the CPU shares; critical thread {res['profile']['critical_thread']}, lock wait {res['critical_lockwait_pct']:.1f}% of its timeline", '',
      '| layer | status | % of CPU | % of critical thread | top 3 functions |', '|---|---|---:|---:|---|']
for r in layer_rows:
    md.append(f"| {r['layer']} | {r['status']} | {r['cpu_pct']:.1f} | {r['critical_pct']:.1f} | " + '; '.join(f"`{f[:60]}` {v}" for f, v in r['top_functions']) + ' |')
am = res['amdahl']
md += ['', f"replaced layers: {rep_pct:.1f}% of CPU. Without them: {am['remaining_cpu_ms_per_doc_without_replaced_layers']:.3f} ms CPU/doc (from {am['cpu_ms_per_doc']:.3f}); CPU-bound ceiling on {am['physical_cores']} physical cores {am['docs_per_s_cpu_bound_without_replaced_layers']:.0f} docs/s; without the Xapian rows only {am['docs_per_s_cpu_bound_without_xapian_only']:.0f} docs/s; front end (HTTP+decode) one-thread ceiling {am['docs_per_s_front_end_one_thread'] or 0:.0f} docs/s", '', f"engine share (rows replaced S1): {engine_cpu_ms_doc:.4f} ms CPU/doc by samples ({res['profile']['engine_share']['cpu_ms_per_doc_by_smoke']:.4f} by smoke CPU x share), {engine_crit_ms_doc:.4f} ms/doc on the critical thread", '', 'thread groups (samples: busy / lockwait / idle):', '']
for g, c in sorted(tg.items(), key=lambda kv: -kv[1]['busy']):
    if c['busy'] or c['lockwait']: md.append(f"- {g}: {c['busy']} / {c['lockwait']} / {c['idle']}")
open(os.path.join(a.out, f'{a.label}.md'), 'w').write('\n'.join(md) + '\n')
print('\n'.join(md))
if res['invalid']: print(f"profile_bulk.sh: {a.label}: INVALID PROFILE ({res['invalid']}); discard this run", file=sys.stderr); sys.exit(3)
PY
