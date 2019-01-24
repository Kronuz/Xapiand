from __future__ import print_function

import json
import msgpack

if __name__ == '__main__':
    accounts_json = open('accounts.json')
    accounts_msgpack = open('accounts.msgpack', 'w')
    accounts_ndjson = open('accounts.ndjson', 'w')
    accounts_http = open('accounts.http', 'w')
    accounts = json.load(accounts_json)
    msgpack.dump(accounts, accounts_msgpack)
    for i, a in enumerate(accounts, 1):
        aj = json.dumps(a)
        print(aj, file=accounts_ndjson)
        print("""PUT /bank/{} HTTP/1.1
Host: localhost:8880
Content-Type: application/json
Content-Length: {}

{}""".format(i, len(aj), aj), file=accounts_http)
