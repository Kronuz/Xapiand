from __future__ import print_function

import json
import msgpack

if __name__ == '__main__':
    accounts = json.load(open('accounts.json'))
    msgpack.dump(accounts, open('accounts.msgpack', 'w'))
    with open('accounts.http', 'w') as fp:
        for i, a in enumerate(accounts, 1):
            aj = json.dumps(a)
            print("""PUT /bank/{} HTTP/1.1
Host: localhost:8880
Content-Type: application/json
Content-Length: {}

{}""".format(i, len(aj), aj), file=fp)
