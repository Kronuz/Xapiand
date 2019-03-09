#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2019 Dubalu LLC
# Copyright (c) 2017 Elasticsearch
#
# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to you under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import print_function

import logging
from dateutil.parser import parse as parse_date

from xapiand import Xapiand


def print_search_stats(results):
    print('=' * 80)
    print('Total %d found in %s' % (results['total'], results['took']))
    print('-' * 80)


def print_hits(results):
    " Simple utility function to print results of a search query. "
    print_search_stats(results)
    for hit in results['hits']:
        # get created date for a repo and fallback to authored_date for a commit
        created_at = parse_date(hit.get('created_at', hit['authored_date']))
        print('%s (%s): %s' % (hit['_id'], created_at.strftime('%Y-%m-%d'), hit['description'].split('\n')[0]))
    print('=' * 80)
    print()


# get trace logger and set level
tracer = logging.getLogger('xapiand.trace')
tracer.setLevel(logging.INFO)
tracer.addHandler(logging.FileHandler('/tmp/xapiand_trace.log'))
# instantiate Xapiand client, connects to localhost:8880 by default
client = Xapiand()

print('Empty search:')
print_hits(client.search(index='git'))

print('Find commits that says "fix" without touching tests:')
result = client.search(
    index='git',
    body={
        '_query': {
            '_and_not': [
                {
                    'description': 'version'
                },
                {
                    'files': 'adjust'
                }
            ]
        }
    }
)
print_hits(result)

print('Last 8 Commits for xapiand-py:')
result = client.search(
    index='git',
    body={
        '_query': {
            'repository': 'xapiand-py'
        },
        '_sort': [
            {
                'committed_date': {
                    '_order': 'desc'
                }
            }
        ],
        '_limit': 8
    }
)
print_hits(result)

print('Stats for top 10 committers:')
result = client.search(
    index='git',
    body={
        "_limit": 0,
        "_check_at_least": 100000,
        "_aggs": {
            "committers": {
                "_values": {
                    "_field": "committer.email"
                },
                "_aggs": {
                    "line_stats": {
                        "_stats": {
                            "_field": "stats.lines"
                        }
                    }
                }
            }
        }
    }
)

print_search_stats(result)
for committer in result['aggregations']['committers']:
    print('%15s: %3d commits changing %6d lines' % (
        committer['_key'], committer['_doc_count'], committer['line_stats']['_sum']))
print('=' * 80)
