#!/usr/bin/env python
from __future__ import print_function

import os
import re


PARSER_RE = re.compile(r'\n```(js(?:on)?)(.*?)\n```|\ntitle: ([^\n]+)|\n(#+)\s*([^\n]+)|\n\*\*(.+?)\*\*', re.DOTALL)


index = {}
all_tests = []


for path, dirs, files in os.walk('docs/_docs'):
    # print(path, files)
    for f in files:
        if not f.endswith('.md'):
            continue
        filename = os.path.join(path, f)
        filename_path, _ = os.path.splitext(filename)
        data = open(filename).read()
        fnp = filename_path
        base_titles = []
        while fnp:
            if fnp in index:
                base_titles.insert(0, index.get(fnp))
            fnp = os.path.dirname(fnp)
        context = {
            'titles': [(0, title) for title in base_titles]
        }
        # print(filename_path, base_titles)
        def process(m):
            groups = m.groups()
            # print(groups)
            if groups[0] == 'json':
                if context:
                    all_tests.append(dict(context))
                titles = context.pop('titles', [])
                context.clear()
                context['titles'] = titles
                context['request'] = groups[1]
            elif groups[0] == 'js':
                context.setdefault('tests', []).append(groups[1])
            elif groups[2]:
                index[filename_path] = groups[2]
                context['titles'].append((0, groups[2]))
                context.pop('name', None)
            elif groups[3]:
                level = len(groups[3])
                context['titles'] = [title for title in context.get('titles', []) if title[0] < level]
                context['titles'].append((level, groups[4]))
                context.pop('name', None)
            elif groups[5]:
                context['name'] = groups[5]
        PARSER_RE.sub(process, data)

for test in all_tests:
    print(test)
