#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
# Copyright © 2015-2020 Dubalu LLC
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
