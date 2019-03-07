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

from .bulk import bulk
from .errors import ScanError

import logging


logger = logging.getLogger('xapiand.helpers')


def scan(client, query=None, scroll='5m', raise_on_error=True,
         preserve_order=False, size=1000, request_timeout=None, clear_scroll=True,
         scroll_kwargs=None, **kwargs):
    """
    Simple abstraction on top of the
    :meth:`~xapiand.Xapiand.scroll` api - a simple iterator that
    yields all hits as returned by underlining scroll requests.

    By default scan does not return results in any pre-determined order. To
    have a standard order in the returned documents (either by score or
    explicit sort definition) when scrolling, use ``preserve_order=True``. This
    may be an expensive operation and will negate the performance benefits of
    using ``scan``.

    :arg client: instance of :class:`~xapiand.Xapiand` to use
    :arg query: body for the :meth:`~xapiand.Xapiand.search` api
    :arg scroll: Specify how long a consistent view of the index should be
        maintained for scrolled search
    :arg raise_on_error: raises an exception (``ScanError``) if an error is
        encountered (some shards fail to execute). By default we raise.
    :arg preserve_order: don't set the ``search_type`` to ``scan`` - this will
        cause the scroll to paginate with preserving the order. Note that this
        can be an extremely expensive operation and can easily lead to
        unpredictable results, use with caution.
    :arg size: size (per shard) of the batch send at each iteration.
    :arg request_timeout: explicit timeout for each call to ``scan``
    :arg clear_scroll: explicitly calls delete on the scroll id via the clear
        scroll API at the end of the method on completion or error, defaults
        to true.
    :arg scroll_kwargs: additional kwargs to be passed to
        :meth:`~xapiand.Xapiand.scroll`

    Any additional keyword arguments will be passed to the initial
    :meth:`~xapiand.Xapiand.search` call::

        scan(client,
            query={"query": {"match": {"title": "python"}}},
            index="orders-*",
            doc_type="books"
        )

    """
    scroll_kwargs = scroll_kwargs or {}

    if not preserve_order:
        query = query.copy() if query else {}
        query["sort"] = "_doc"
    # initial search
    resp = client.search(body=query, scroll=scroll, size=size,
                         request_timeout=request_timeout, **kwargs)

    scroll_id = resp.get('_scroll_id')
    if scroll_id is None:
        return

    try:
        first_run = True
        while True:
            # if we didn't set search_type to scan initial search contains data
            if first_run:
                first_run = False
            else:
                resp = client.scroll(scroll_id, scroll=scroll,
                                     request_timeout=request_timeout,
                                     **scroll_kwargs)

            for hit in resp['hits']['hits']:
                yield hit

            # check if we have any errrors
            if resp["_shards"]["successful"] < resp["_shards"]["total"]:
                logger.warning(
                    "Scroll request has only succeeded on %d shards out of %d.",
                    resp['_shards']['successful'], resp['_shards']['total']
                )
                if raise_on_error:
                    raise ScanError(
                        scroll_id,
                        "Scroll request has only succeeded on %d shards out of %d." % (
                            resp['_shards']['successful'], resp['_shards']['total']
                        )
                    )

            scroll_id = resp.get('_scroll_id')
            # end of scroll
            if scroll_id is None or not resp['hits']['hits']:
                break
    finally:
        if scroll_id and clear_scroll:
            client.clear_scroll(body={'scroll_id': [scroll_id]}, ignore=(404, ))


def reindex(client, source_index, target_index, query=None, target_client=None,
            chunk_size=500, scroll='5m', scan_kwargs={}, bulk_kwargs={}):
    """
    Reindex all documents from one index that satisfy a given query
    to another, potentially (if `target_client` is specified) on a different cluster.
    If you don't specify the query you will reindex all the documents.

    Since ``2.3`` a :meth:`~xapiand.Xapiand.reindex` api is
    available as part of xapiand itself. It is recommended to use the api
    instead of this helper wherever possible. The helper is here mostly for
    backwards compatibility and for situations where more flexibility is
    needed.

    .. note::

        This helper doesn't transfer mappings, just the data.

    :arg client: instance of :class:`~xapiand.Xapiand` to use (for
        read if `target_client` is specified as well)
    :arg source_index: index (or list of indices) to read documents from
    :arg target_index: name of the index in the target cluster to populate
    :arg query: body for the :meth:`~xapiand.Xapiand.search` api
    :arg target_client: optional, is specified will be used for writing (thus
        enabling reindex between clusters)
    :arg chunk_size: number of docs in one chunk sent to the server (default: 500)
    :arg scroll: Specify how long a consistent view of the index should be
        maintained for scrolled search
    :arg scan_kwargs: additional kwargs to be passed to
        :func:`~xapiand.helpers.scan`
    :arg bulk_kwargs: additional kwargs to be passed to
        :func:`~xapiand.helpers.bulk`
    """
    target_client = client if target_client is None else target_client

    docs = scan(
        client,
        query=query,
        index=source_index,
        scroll=scroll,
        **scan_kwargs
    )

    def _change_doc_index(hits, index):
        for h in hits:
            h['_index'] = index
            if 'fields' in h:
                h.update(h.pop('fields'))
            yield h

    kwargs = {
        'stats_only': True,
    }
    kwargs.update(bulk_kwargs)
    return bulk(target_client,
                _change_doc_index(docs, target_index),
                chunk_size=chunk_size,
                **kwargs)
