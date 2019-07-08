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

from ..exceptions import TransportError
from ..helpers import bulk, streaming_bulk, parallel_bulk
from ..utils import NamespacedClient, query_params, make_url, SKIP_IN_PATH


class IndicesClient(NamespacedClient):
    @query_params()
    def ping(self, params=None):
        """
        Returns True if the cluster is up, False otherwise.
        """
        try:
            return self.transport.perform_request('HEAD', '',
                params=params)
        except TransportError:
            return False

    @query_params('timeout')
    def create(self, index, body=None, params=None):
        """
        Create an index in Xapiand.

        :arg index: The name of the index
        :arg body: The configuration for the index (`_settings` and `_schema`)
        :arg timeout: Explicit operation timeout
        """
        return self.transport.perform_request('UPDATE', make_url(index),
            params=params, body=body)

    @query_params('timeout')
    def commit(self, index=None, params=None):
        """
        Explicitly commit one or more index, making all operations performed
        since the last commit available for search.

        :arg index: A comma-separated list of index names; use `_all` or empty
            string to perform the operation on all indices
        :arg timeout: Explicit operation timeout
        """
        return self.transport.perform_request('COMMIT', make_url(index),
            params=params)

    @query_params('timeout')
    def open(self, index, params=None):
        """
        Open a closed index to make it available for search.

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('OPEN', make_url(index),
            params=params)

    @query_params('timeout')
    def close(self, index, params=None):
        """
        Close an index to remove it's overhead from the cluster. Closed index
        is blocked for read/write operations.

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('CLOSE', make_url(index),
            params=params)

    @query_params('timeout')
    def delete(self, index, params=None):
        """
        Delete an index in Xapiand

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('DELETE', make_url(index),
            params=params)

    @query_params('q', 'query', 'refresh', 'timeout')
    def count(self, index=None, body=None, params=None):
        """
        Execute a query and get the number of matches for that query.

        :arg index: A comma-separated list of indices to restrict the results
        :arg body: A query to restrict the results specified with the Query DSL
            (optional)
        :arg q: Query in the query string syntax
        :arg timeout: Explicit operation timeout
        """
        if not index:
            index = '*'
        return self.transport.perform_request('COUNT', make_url(index),
            params=params, body=body)

    @query_params('q', 'query', 'offset', 'limit', 'sort', 'selector', 'refresh', 'timeout')
    def search(self, index=None, body=None, params=None):
        """
        Execute a search query and get back search hits that match the query.

        :arg index: A comma-separated list of index names to search; use `*`
            or empty string to perform the operation on all indices
        :arg body: The search definition using the Query DSL (optional)
        :arg q: Query in the query string syntax
        :arg offset: Starting offset for response (default: 0)
        :arg limit: Number of hits to return (default: 10)
        :arg sort: A comma-separated list of <field>:<direction> pairs
        :arg timeout: Explicit operation timeout
        """
        if not index:
            index = '*'
        return self.transport.perform_request('SEARCH', make_url(index),
            params=params, body=body)

    def _restore(self, body, index, params=None):
        for param in (index, body):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")

        content_type, body = self.transport.serializer.nddumps(body)
        return self.transport.perform_request('RESTORE', make_url(index),
            params=params, body=body,
            headers={'content-type': content_type})

    @query_params('timeout')
    def streaming_restore(self, index, body,
                          chunk_size=500, max_chunk_bytes=100 * 1024 * 1024,
                          raise_on_error=True, raise_on_exception=True, max_retries=0,
                          initial_backoff=2, max_backoff=600, yield_ok=True, params=None):
        """
        Perform many index operations in a single API call.
        Streaming restore consumes objects from the iterable passed in and
        yields results per object. For non-streaming usecases use
        :func:`~xapiand.client.restore` which is a wrapper around streaming
        bulk that returns summary information about the bulk operation once the
        entire input is consumed and sent.

        If you specify ``max_retries`` it will also retry any documents that were
        rejected with a ``429`` status code. To do this it will wait (**by calling
        time.sleep which will block**) for ``initial_backoff`` seconds and then,
        every subsequent rejection for the same chunk, for double the time every
        time up to ``max_backoff`` seconds.

        :arg index: The name of the index
        :arg body: The list of documents to index, they can specify ``_id``
        :arg chunk_size: number of docs in one chunk sent to the server (default: 500)
        :arg max_chunk_bytes: the maximum size of the request in bytes (default: 100MB)
        :arg raise_on_error: raise ``BulkIndexError`` containing errors (as `.errors`)
            from the execution of the last chunk when some occur. By default we raise.
        :arg raise_on_exception: if ``False`` then don't propagate exceptions from
            call to ``bulk`` and just report the items that failed as failed.
        :arg max_retries: maximum number of times a document will be retried when
            ``429`` is received, set to 0 (default) for no retries on ``429``
        :arg initial_backoff: number of seconds we should wait before the first
            retry. Any subsequent retries will be powers of ``initial_backoff *
            2**retry_number``
        :arg max_backoff: maximum number of seconds a retry will wait
        :arg yield_ok: if set to False will skip successful documents in the output
        :arg timeout: Explicit operation timeout
        """
        return streaming_bulk(
            self._restore,
            body,
            self.transport.serializer,
            chunk_size=chunk_size,
            max_chunk_bytes=max_chunk_bytes,
            raise_on_error=raise_on_error,
            raise_on_exception=raise_on_exception,
            max_retries=max_retries,
            initial_backoff=initial_backoff,
            max_backoff=max_backoff,
            yield_ok=yield_ok,
            params=params,
            index=index,
        )

    @query_params('timeout')
    def restore(self, index, body, stats_only=False, params=None, *args, **kwargs):
        """
        Perform many index operations in a single API call. It returns a tuple
        with summary information - number of successfully processed objects and
        either list of errors or number of errors if ``stats_only`` is set to
        ``True``. Note that by default we raise a ``BulkIndexError`` when we
        encounter an error so options like ``stats_only`` only apply when
        ``raise_on_error`` is set to ``False``.

        When errors are being collected original document data is included in the
        error dictionary which can lead to an extra high memory usage. If you need
        to process a lot of data and want to ignore/collect errors please consider
        using the :func:`~xapiand.helpers.streaming_bulk` helper which will
        just return the errors and not store them in memory.

        :arg index: The name of the index
        :arg body: The list of documents to index, they can specify ``_id``
        :arg stats_only: if `True` only report number of successful/failed
            operations instead of just number of successful and a list of error responses
        :arg timeout: Explicit operation timeout

        Any additional keyword arguments will be passed to
        :func:`~xapiand.helpers.streaming_bulk` which is used to execute
        the operation, see :func:`~xapiand.helpers.streaming_bulk` for more
        accepted parameters.
        """
        return bulk(
            self._restore,
            body,
            self.transport.serializer,
            stats_only=stats_only,
            params=params,
            index=index,
            *args, **kwargs
        )

    @query_params('timeout')
    def parallel_restore(self, index, body,
                         chunk_size=500, max_chunk_bytes=100 * 1024 * 1024,
                         thread_count=4, queue_size=4,
                         raise_on_error=True, raise_on_exception=True,
                         params=None):
        """
        Parallel version of the bulk helper run in multiple threads at once.

        :arg index: The name of the index
        :arg body: The list of documents to index, they can specify ``_id``
        :arg thread_count: size of the threadpool to use for the bulk requests
        :arg chunk_size: number of docs in one chunk sent to the server (default: 500)
        :arg max_chunk_bytes: the maximum size of the request in bytes (default: 100MB)
        :arg raise_on_error: raise ``BulkIndexError`` containing errors (as `.errors`)
            from the execution of the last chunk when some occur. By default we raise.
        :arg raise_on_exception: if ``False`` then don't propagate exceptions from
            call to ``bulk`` and just report the items that failed as failed.
        :arg queue_size: size of the task queue between the main thread (producing
            chunks to send) and the processing threads.
        :arg timeout: Explicit operation timeout
        """
        return parallel_bulk(
            self._restore,
            body,
            self.transport.serializer,
            chunk_size=chunk_size,
            max_chunk_bytes=max_chunk_bytes,
            thread_count=thread_count,
            queue_size=queue_size,
            raise_on_error=raise_on_error,
            raise_on_exception=raise_on_exception,
            params=params,
            index=index,
        )
