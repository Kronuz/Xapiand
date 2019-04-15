#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function

import os
import sys
import json
import msgpack
from collections import OrderedDict


def main():
    if len(sys.argv) != 2:
        print("usage: {} <file.json>".format(sys.argv[0]))
        sys.exit(64)
    name, ext = os.path.splitext(sys.argv[1])
    json_content = open(name + '.json')
    msgpack_content = open(name + '.msgpack', 'wb')
    ndjson_content = open(name + '.ndjson', 'w')
    http_content = open(name + '.http', 'w')
    content = json.load(json_content, object_pairs_hook=OrderedDict)
    for i, a in enumerate(content, 1):
        msgpack_content.write(msgpack.dumps(a))
        aj = json.dumps(a)
        print(aj, file=ndjson_content)
        print("""PUT /bank/{} HTTP/1.1
Host: localhost:8880
Content-Type: application/json
Content-Length: {}

{}""".format(i, len(aj), aj), file=http_content)

if __name__ == '__main__':
    main()
