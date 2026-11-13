# sampler.py PATTERN OUT [SAMPLE_EVERY]: until killed, every 0.5 s, the runnable (R) threads of the newest process
# matching PATTERN (pgrep -f) and of the whole host, from ps -M; every SAMPLE_EVERY s (0: never) a 1 s `sample`.
import json, subprocess, sys, time

pat, out = sys.argv[1], sys.argv[2]
every = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0


def rows(args):
    r = subprocess.run(["ps", "-M", *args], capture_output=True, text=True).stdout.splitlines()[1:]
    for line in r:
        f = line.split()
        if not f:
            continue
        # a process's first row: USER PID TT %CPU STAT ...; its other threads: PID %CPU STAT ...
        if f[0].isdigit():
            yield f[0], float(f[1]), f[2]
        else:
            yield f[1], float(f[3]), f[4]


pid, last_sample = None, 0.0
with open(out, "w") as fo:
    while True:
        t = time.time()
        if pid is None:
            p = subprocess.run(["pgrep", "-n", "-f", pat], capture_output=True, text=True).stdout.split()
            pid = p[0] if p else None
        if pid is not None:
            node = list(rows(["-p", pid]))
            if not node:
                pid = None
            else:
                host = list(rows(["-A"]))
                rec = {
                    "t": t,
                    "node_threads": len(node),
                    "node_r": sum(s.startswith("R") for _, _, s in node),
                    "node_cpu": round(sum(c for _, c, _ in node), 1),
                    "host_r": sum(s.startswith("R") for _, _, s in host) - 1,  # less ps itself
                    "other_r": sum(s.startswith("R") for p_, _, s in host if p_ != pid) - 1,
                }
                if every and t - last_sample >= every:
                    last_sample = t
                    f = f"{out}.{int(t)}.sample.txt"
                    subprocess.run(["sample", pid, "1", "-mayDie", "-file", f], capture_output=True)
                    rec["sample"] = f
                fo.write(json.dumps(rec) + "\n")
                fo.flush()
        time.sleep(max(0.0, 0.5 - (time.time() - t)))
