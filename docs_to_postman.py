#!/usr/bin/env python
# python docs_to_postman.py | newman run /dev/stdin
from __future__ import print_function

import re
import os
import sys
import json
import email
import urllib
import urlparse


PARSER_RE = re.compile(r'\n```\s*([a-z]*)(.*?)\n```|\ntitle: ([^\n]+)|\n(#+)\s*([^\n]+)|title="(.+?)"', re.DOTALL)

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def main():
    index = {}
    all_tests = []

    for path, dirs, files in os.walk(os.path.join(BASE_DIR, 'docs')):
        for f in files:
            # print(path, f)
            if not f.endswith('.md'):
                continue
            filename = os.path.join(path, f)
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
                    context['titles'].insert(0, (0, index.get(fnp)))
                fnp = os.path.dirname(fnp)
            # print(filename_path, base_titles)

            def process(m):
                groups = m.groups()
                # print(groups)
                if groups[0] == 'json':
                    # Flush:
                    if context and 'request' in context:
                        all_tests.append(dict(context))
                    context.clear()
                    context.update(file_context)
                    context['request'] = groups[1].strip()
                elif groups[0] == 'js':
                    context.setdefault('tests', []).append(groups[1].strip())
                elif groups[2]:
                    # Flush:
                    if context and 'request' in context:
                        all_tests.append(dict(context))
                    context.clear()
                    context.update(file_context)
                    # Add title:
                    index[filename_path] = groups[2]
                    context['titles'].append((0, groups[2]))
                elif groups[3]:
                    # Flush:
                    if context and 'request' in context:
                        all_tests.append(dict(context))
                    context.clear()
                    context.update(file_context)
                    # Add title:
                    level = len(groups[3])
                    context['titles'] = [title for title in context.get('titles', []) if title[0] < level]
                    context['titles'].append((level, groups[4]))
                elif groups[5]:
                    context['name'] = urllib.unquote(groups[5])
            PARSER_RE.sub(process, data)

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
        for _, name in test["titles"]:
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
        for i, line in enumerate(test["request"].split('\n')):
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
                        raise ValueError("Malformed header in {} ({}): {}".format(test["filename"], ' / '.join(title), repr(line)))
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
        qs = urlparse.parse_qs(parsed.query)
        for q in ("commit", "echo", "volatile"):
            if q not in qs:
                qs[q] = None
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
        name = test.get("name")
        item = {
            "request": request,
        }
        if name:
            item["name"] = name
        items.append(item)
        tests = test.get("tests")
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
