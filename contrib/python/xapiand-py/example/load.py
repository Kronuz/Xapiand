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

from os.path import dirname, basename, abspath
from datetime import datetime
import logging
import argparse

import git

from xapiand import Xapiand
from xapiand.exceptions import TransportError


def create_git_index(client, index):
    # we will use user on several places
    user_mapping = {
        'name': {
            '_type': 'text',
            'keyword': {
                '_type': 'keyword'
            },
        }
    }

    create_index_body = {
        'schema': {
            'repository': {
                '_type': 'keyword'
            },
            'author': user_mapping,
            'authored_date': {
                '_type': 'date'
            },
            'committer': user_mapping,
            'committed_date': {
                '_type': 'date'
            },
            'parent_shas': {
                '_type': 'keyword'
            },
            'description': {
                '_type': 'text',
                '_language': 'en'
            },
            'files': {
                '_type': 'array/keyword',
            }
        }
    }

    # create empty index
    try:
        client.indices.create(
            index=index,
            body=create_index_body,
        )
    except TransportError as e:
        # ignore already existing index
        if e.error == 'index_already_exists_exception':
            pass
        else:
            raise


def parse_commits(head, name):
    """
    Go through the git repository log and generate a document per commit
    containing all the metadata.
    """
    for commit in head.traverse():
        yield {
            '_id': commit.hexsha,
            'repository': name,
            'committed_date': datetime.fromtimestamp(commit.committed_date),
            'committer': {
                'name': commit.committer.name,
                'email': commit.committer.email,
            },
            'authored_date': datetime.fromtimestamp(commit.authored_date),
            'author': {
                'name': commit.author.name,
                'email': commit.author.email,
            },
            'description': commit.message,
            'parent_shas': [p.hexsha for p in commit.parents],
            # we only care about the filenames, not the per-file stats
            'files': list(commit.stats.files),
            'stats': commit.stats.total,
        }


def load_repo(client, path=None, index='git'):
    """
    Parse a git repository with all it's commits and load it into xapiand
    using `client`. If the index doesn't exist it will be created.
    """
    path = dirname(abspath(__file__)) if path is None else path

    while True:
        try:
            repo = git.Repo(path)
        except git.exc.InvalidGitRepositoryError:
            parent_path = dirname(path)
            if parent_path == path:
                raise
            path = parent_path
    repo_name = basename(path)

    create_git_index(client, index)

    # we let the streaming bulk continuously process the commits as they come
    # in - since the `parse_commits` function is a generator this will avoid
    # loading all the commits into memory
    for status, result in client.streaming_restore(
        index,
        parse_commits(repo.refs.master.commit, repo_name),
        chunk_size=50,  # keep the batch sizes small for appearances only
    ):
        doc_id = '/%s/%s' % (index, result['_id'])
        # process the information from ES whether the document has been
        # successfully indexed
        if 200 <= status < 300:
            print(doc_id)
        else:
            print('Failed to index document %s: %r' % (doc_id, result))


# we manually update some documents to add additional information
UPDATES = [
    {
        '_op_type': 'update',
        '_id': '8d6dfaefa26b5352816093c5f631439add711bf4',
        'initial_commit': True
    },
    {
        '_op_type': 'update',
        '_id': '1085573f944d3a801d06762a875a29f9794e003d',
        'release': '5.0.0'
    },
]

if __name__ == '__main__':
    # get trace logger and set level
    tracer = logging.getLogger('xapiand.trace')
    tracer.setLevel(logging.INFO)
    tracer.addHandler(logging.FileHandler('/tmp/xapiand_trace.log'))

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-H", "--host",
        action="store",
        default="localhost:8880",
        help="The Xapiand host you wish to connect to. (Default: localhost:8880)")
    parser.add_argument(
        "-p", "--path",
        action="store",
        default=None,
        help="Path to git repo. Commits used as data to load into Xapiand. (Default: None")

    args = parser.parse_args()

    # instantiate Xapiand client, connects to localhost:8880 by default
    client = Xapiand(args.host)

    # we load the repo and all commits
    load_repo(client, path=args.path)

    # run the bulk operations
    success, _ = client.restore('git', UPDATES)
    print('Performed %d actions' % success)

    # we can now make docs visible for searching
    client.indices.commit(index='git')

    # now we can retrieve the documents
    initial_commit = client.get(index='git', id='8d6dfaefa26b5352816093c5f631439add711bf4')
    print('%s: %s' % (initial_commit['_id'], initial_commit['committed_date']))

    # and now we can count the documents
    print(client.count(index='git')['count'], 'documents in index')
