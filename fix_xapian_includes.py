#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import print_function

import os
import re


def main():
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'src'))
    prefix_len = len(os.path.dirname(os.path.abspath('xapian'))) + 1
    for dirpath, dirnames, filenames in os.walk('xapian'):
        if dirpath not in ('.deps', '.libs'):
            for filename in filenames:
                printed_path = [False]
                root, ext = os.path.splitext(filename)
                if ext in ('.cc', '.h', '.lemony', '.lt', '.tcl') or filename in ('generate-exceptions',):
                    def fn(m):
                        include = m.group(3)
                        if include and include not in ('config.h', 'xapian.h'):
                            for q in (dirpath, 'xapian/common', 'xapian', ''):
                                tmp = os.path.abspath(os.path.join(q, include))
                                tmp = tmp[prefix_len:]
                                if os.path.exists(tmp) or tmp in (
                                    'xapian/languages/sbl-dispatch.h',
                                    'xapian/languages/allsnowballheaders.h',
                                    'xapian/queryparser/queryparser_token.h',
                                    'xapian/unicode/c_istab.h',
                                    'xapian/errordispatch.h',
                                    'xapian/error.h',
                                    'xapian/version.h',
                                ):
                                    # parents = ''
                                    # basepath = dirpath
                                    # while basepath:
                                    #     if tmp.startswith(basepath + '/'):
                                    #         tmp = parents + tmp[len(basepath) + 1:]
                                    #         break
                                    #     parents += '../'
                                    #     basepath = os.path.dirname(basepath)
                                    include = tmp
                                    break
                            else:
                                if not printed_path[0]:
                                    printed_path[0] = True
                                    print(path)
                                print("  Include not found:", m.group(2))
                        elif not include:
                            include = m.group(4)
                        if ext == '.tcl':
                            return m.group(1) + '\\"' + include + '\\"'
                        else:
                            return m.group(1) + '"' + include + '"'
                    path = os.path.join(dirpath, filename)
                    with open(path) as f:
                        file = f.read()
                    file = re.sub(r'(\n\s*#\s*include )("([^"]+)"|<(config\.h|xapian\.h|xapian/[^>]+)>)', fn, file)
                    with open(path, 'w') as f:
                        f.write(file)

    def fn(m):
        include = m.group(3)
        if not os.path.exists(include):
            print('xapian.h')
            print("  Include not found:", m.group(2))
        return m.group(1) + '"' + include + '"'
    with open('xapian.h') as f:
        file = f.read()
    file = re.sub(r'(\n\s*#\s*include )(<(xapian/[^>]+)>)', fn, file)
    with open('xapian.h', 'w') as f:
        f.write(file)

    with open('xapian/languages/compiler/generator.c') as f:
        file = f.read()
    file = file.replace('<config.h>', '\\"config.h\\"')
    file = file.replace('\\"steminternal.h\\"', '\\"xapian/languages/steminternal.h\\"')
    with open('xapian/languages/compiler/generator.c', 'w') as f:
        f.write(file)

    with open('xapian/generate-exceptions') as f:
        file = f.read()
    file = file.replace('include/xapian/', 'xapian/')
    with open('xapian/generate-exceptions', 'w') as f:
        f.write(file)


if __name__ == '__main__':
    main()
