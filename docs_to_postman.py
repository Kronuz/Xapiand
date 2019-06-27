#!/usr/bin/env python
# python docs_to_postman.py | newman run /dev/stdin
from __future__ import print_function

import re
import os
import sys
import json
import copy
import email
import urlparse


PARSER_RE = re.compile(r'\n```\s*([a-z]*)(.*?)\n```|\n(#+)\s*([^\n]+)|\n([a-z]+)\s*:\s*([^\n]+)', re.DOTALL)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def parse_filename(filename, index, all_tests):
    filename_path, _ = os.path.splitext(filename)
    data = open(filename).read()
    fnp = filename_path
    file_context = {
        'filename': filename,
        'titles': [],
    }
    context = {}
    context.update(file_context)
    while fnp and fnp != BASE_DIR and fnp != '/':
        if fnp in index:
            file_context['titles'].insert(0, (0, index.get(fnp)))
        fnp = os.path.dirname(fnp)
    # print(filename_path, base_titles)

    def process(m):
        groups = m.groups()
        # print(groups)
        if groups[0] == 'json':
            # Flush:
            if context and 'request' in context:
                all_tests.append(copy.deepcopy(context))
            context.clear()
            context.update(file_context)
            context['request'] = groups[1].strip()
        elif groups[0] == 'js':
            context.setdefault('tests', []).append(groups[1].strip())
        elif groups[2]:
            # Flush:
            if context and 'request' in context:
                all_tests.append(copy.deepcopy(context))
            context.clear()
            context.update(file_context)
            # Add title:
            level = len(groups[2])
            file_context['titles'] = [title for title in file_context.get('titles', []) if title[0] < level]
            file_context['titles'].append((level, groups[3]))
            # Clear description:
            file_context.pop('description', None)
            context.pop('description', None)
        elif groups[5]:
            name = groups[4]
            if name == 'description':
                # Persist description:
                file_context[name] = groups[5]
                context[name] = groups[5]
            elif name == 'title':
                # Flush:
                if context and 'request' in context:
                    all_tests.append(copy.deepcopy(context))
                context.clear()
                context.update(file_context)
                # Add title:
                index[filename_path] = groups[5]
                file_context['titles'].append((1, groups[5]))
            else:
                context[name] = groups[5]
    PARSER_RE.sub(process, data)
    # Flush:
    if context and 'request' in context:
        all_tests.append(copy.deepcopy(context))


def parse_directory(directory, index, all_tests):
    for path, dirs, files in os.walk(directory):
        for f in files:
            if f.endswith('.md'):
                # print(path, f)
                filename = os.path.join(path, f)
                parse_filename(filename, index, all_tests)


def main():
    index = {}
    all_tests = []

    if len(sys.argv) > 1:
        for arg in sys.argv:
            if os.path.abspath(arg) != os.path.abspath(__file__):
                if os.path.isdir(arg):
                    parse_directory(arg, index, all_tests)
                else:
                    parse_filename(arg, index, all_tests)
    else:
        directory = os.path.join(BASE_DIR, 'docs')
        parse_directory(directory, index, all_tests)

    # print(json.dumps(all_tests, indent=4))

    collection = {
        "info": {
            "name": "Xapiand",
            "description": "Xapiand is A Modern Highly Available Distributed RESTful Search and Storage Engine built for the Cloud and with Data Locality in mind.",
            "schema": "https://schema.getpostman.com/json/collection/v2.1.0/collection.json"
        },
        "variable": [
            {
                "key": "domain",
                "value": "localhost:8880",
                "type": "string"
            }
        ],
        "item": [],
    }

    for test in all_tests:
        items = collection["item"]
        title = []
        for _, name in test['titles']:
            for item in items:
                if item.get("name") == name:
                    break
            else:
                item = {
                    "name": name,
                    "item": []
                }
                items.append(item)
            title.append(item["name"])
            items = item["item"]
        url = ""
        method = None
        body = []
        headers = email.Message.Message()
        for i, line in enumerate(test['request'].split('\n')):
            line = line.strip()
            if i == 0:
                method, _, url = line.partition(' ')
                if not re.match(r'[A-Z]+', method):
                    method = None
                    break
            elif not body:
                if not line:
                    body.append(line)
                else:
                    k, _, v = line.partition(':')
                    if not _:
                        raise ValueError("Malformed header in {} ({}): {}".format(test['filename'], ' / '.join(title), repr(line)))
                    headers.add_header(k.strip(), v.strip())
            else:
                body.append(line)
        if not method:
            continue
        for k, v in {
            "Content-Type": "application/json"
        }.items():
            if k not in headers:
                headers[k] = v
        header = []
        for k, v in headers.items():
            header.append({
                "type": "text",
                "key": k,
                "value": v,
            })
        parsed = urlparse.urlparse(url)
        path = parsed.path[1:] if parsed.path.startswith('/') else parsed.path
        path = path.split('/')
        qs = urlparse.parse_qs(parsed.query + '&' + test.get('params', ''))
        for k, v in {
            "commit": None,
            "volatile": None,
            "echo": None,
        }.items():
            if k not in qs:
                qs[k] = v
        query = []
        for k, v in qs.items():
            query.append({
                "key": k,
                "value": v,
            })
        url = {
            "host": [
                "{{domain}}"
            ],
            "path": path,
        }
        if query:
            url["query"] = query
        body = '\n'.join(body).strip()
        request = {
            "method": method,
            "url": url,
        }
        if header:
            request["header"] = header
        if body:
            if body[0] == '@':
                request["body"] = {
                    "mode": "file",
                    "file": {
                        "src": os.path.join(BASE_DIR, 'docs', 'assets', body[1:])
                    }
                }
            else:
                request["body"] = {
                    "mode": "raw",
                    "raw": body,
                }
        name = test.get('description')
        item = {
            "request": request,
        }
        if name:
            item["name"] = name
        items.append(item)
        tests = test.get('tests')
        if tests:
            scripts = []
            item["event"] = [{
                "listen": "test",
                "script": {
                    "type": "text/javascript",
                    "exec": scripts,
                }
            }]
            for test in tests:
                scripts.append(test)
        # print(test)

    json.dump(collection, sys.stdout)
    return 0

if __name__ == '__main__':
    sys.exit(main())
