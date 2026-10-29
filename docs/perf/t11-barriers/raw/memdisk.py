import sys, os, time, shutil, subprocess, http.client, resource
repo = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip()
lines = [l for l in open(os.path.join(repo, 'docs/public/assets/accounts.ndjson'), 'rb').read().split(b'\n') if l.strip()]
body = b'\n'.join(lines * 20) + b'\n'
out = sys.argv[1]; port = 18990
for r in range(int(sys.argv[2])):
    for arm in sys.argv[3:]:
        data = os.path.join(out, f'{arm}_{r}.data'); shutil.rmtree(data, ignore_errors=True); os.makedirs(data)
        env = dict(os.environ, XAPIAND_ENGINE='strata')
        p = subprocess.Popen([f'{repo}/.scratch/bin/xapiand.{arm}', '--port', str(port), '--name', 'benchnode', '--solo', '-D', data], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        for _ in range(240):
            try:
                c = http.client.HTTPConnection('localhost', port, timeout=5); c.request('GET', '/'); c.getresponse().read(); break
            except Exception: time.sleep(.5)
        c = http.client.HTTPConnection('localhost', port, timeout=600)
        c.request('RESTORE', '/bench/?commit', body=body, headers={'Content-Type': 'application/x-ndjson'}); c.getresponse().read()
        n = 0; kb = 0
        for d, _, fs in os.walk(data):
            for f in fs:
                if d.count('strata') or f.endswith(('.seg', '.manifest')) or True:
                    pass
            pass
        du = int(subprocess.check_output(['du', '-sk', data]).split()[0])
        files = sum(len(fs) for _, _, fs in os.walk(data))
        p.terminate(); _, st, ru = os.wait4(p.pid, 0)
        print(f'{arm} run{r}: peak RSS {ru.ru_maxrss/1048576:.0f} MB, dir {du} KB, {files} files', flush=True)
        shutil.rmtree(data, ignore_errors=True)
