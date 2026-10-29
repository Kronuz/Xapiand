import json, statistics as st, sys, os
START = int(os.environ.get("START", "1")); d = sys.argv[1]; rounds = int(sys.argv[2]); arms = sys.argv[3:]
def L(a, r): return json.load(open(f'{d}/{a}_{r}.json'))
M = {'cpu': lambda x: x['profile']['engine_share']['cpu_ms_per_doc_by_smoke'],
     'cpu_samples': lambda x: x['profile']['engine_share']['cpu_ms_per_doc'],
     'crit': lambda x: x['profile']['engine_share']['critical_thread_ms_per_doc'],
     'proc': lambda x: x['smoke']['cpu_ms_per_doc'],
     'dps': lambda x: x['smoke']['docs_per_s'],
     'wall': lambda x: x['smoke']['wall_s']}
def sp(v): return (max(v) - min(v)) / st.mean(v) * 100
for k, f in M.items():
    print(f'== {k}')
    x = [f(L('xapian', r)) for r in range(START, rounds + 1)]
    print(f'  xapian        mean {st.mean(x):.4f} spread {sp(x):5.1f}%')
    base = None
    for a in arms:
        v = [f(L(a, r)) for r in range(START, rounds + 1)]
        pairs = [round(b / c, 3) for b, c in zip(v, x)]
        print(f'  {a:<13} mean {st.mean(v):.4f} spread {sp(v):5.1f}%  /xapian {st.mean(v)/st.mean(x):.3f} pairs {pairs}')
