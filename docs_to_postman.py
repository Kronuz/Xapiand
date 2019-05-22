#!/usr/bin/env python
from __future__ import print_function

import os
import re


PARSER_RE = re.compile(r'\n```(js(?:on)?)(.*?)\n```|\ntitle: ([^\n]+)|\n(#+)\s*([^\n]+)\2|\n\*\*(.+?)\*\*', re.DOTALL)


all_tests = []

for path, dirs, files in os.walk('docs/_docs'):
    for f in files:
        filename = os.path.join(path, f)
        data = open(filename).read()
        context = {}

        def process(m):
            groups = m.groups()
            if groups[0] == 'json':
                if context:
                    all_tests.append(context.copy())
                context.clear()
                context['request'] = groups[1]
            elif groups[0] == 'js':
                context.setdefault('tests', []).append(groups[1])
            elif groups[2]:
                context.setdefault('titles', []).append((0, groups[2]))
            elif groups[3]:
                context.setdefault('titles', []).append((len(groups[3]), groups[4]))
            elif groups[5]:
                context['name'] = groups[5]
        PARSER_RE.sub(process, data)


for test in all_tests:
    print(test)
