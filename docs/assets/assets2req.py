from __future__ import print_function

import json

if __name__ == '__main__':
    with open('accounts.req', 'w') as fp:
        for i, a in enumerate(json.load(open('accounts.json')), 1):
            aj = json.dumps(a)
            print("""PUT /bank/{} HTTP/1.1
Host: localhost:8880
Content-Type: application/json
Content-Length: {}

{}""".format(i, len(aj), aj), file=fp)
