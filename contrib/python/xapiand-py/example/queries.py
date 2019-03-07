#!/usr/bin/env python
from __future__ import print_function

import logging
from dateutil.parser import parse as parse_date

from xapiand import Xapiand


def print_search_stats(results):
    print('=' * 80)
    print('Total %d found in %s' % (results['count'], results['took']))
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
        'query': {
            '_and_not': [
                {
                    'description': 'fix'
                },
                {
                    'files': 'test_elasticsearch'
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
        'query': {
            'repository': 'xapiand-py'
        },
        'sort': [
            {
                'committed_date': {
                    'order': 'desc'
                }
            }
        ],
        'limit': 8
    }
)
print_hits(result)

print('Stats for top 10 committers:')
result = client.search(
    index='git',
    body={
        "_limit": 0,
        "_check_at_least": 1000,
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
        committer['_key'], committer['_doc_count'], committer['line_stats']['sum']))
print('=' * 80)
