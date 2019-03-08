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

from __future__ import unicode_literals
import logging

from ..transport import Transport
from ..exceptions import TransportError
from ..compat import string_types, urlparse, unquote
from ..helpers import bulk, streaming_bulk
from .indices import IndicesClient
from .utils import query_params, _make_path, SKIP_IN_PATH

logger = logging.getLogger('xapiand')


def _normalize_hosts(hosts):
    """
    Helper function to transform hosts argument to
    :class:`~xapiand.Xapiand` to a list of dicts.
    """
    # if hosts are empty, just defer to defaults down the line
    if hosts is None:
        return [{}]

    # passed in just one string
    if isinstance(hosts, string_types):
        hosts = [hosts]

    out = []
    # normalize hosts to dicts
    for host in hosts:
        if isinstance(host, string_types):
            if '://' not in host:
                host = '//%s' % host

            parsed_url = urlparse(host)
            h = {'host': parsed_url.hostname}

            if parsed_url.port:
                h['port'] = parsed_url.port

            if parsed_url.scheme == 'https':
                h['port'] = parsed_url.port or 443
                h['use_ssl'] = True

            if parsed_url.username or parsed_url.password:
                h['http_auth'] = '%s:%s' % (unquote(parsed_url.username),
                                            unquote(parsed_url.password))

            if parsed_url.path and parsed_url.path != '/':
                h['url_prefix'] = parsed_url.path

            out.append(h)
        else:
            out.append(host)
    return out


class Xapiand(object):
    """
    Xapiand low-level client. Provides a straightforward mapping from
    Python to Xapiand RESTful endpoints.

    The instance has attribute ``indices`` that provides access to instances of
    :class:`~xapiand.client.IndicesClient`,

    This is the preferred (and only supported) way to get access to those
    classes and their methods.

    You can specify your own connection class which should be used by providing
    the ``connection_class`` parameter::

        # create connection to localhost using the ThriftConnection
        client = Xapiand(connection_class=ThriftConnection)

    If you want to turn on :ref:`sniffing` you have several options (described
    in :class:`~xapiand.Transport`)::

        # create connection that will automatically inspect the cluster to get
        # the list of active nodes. Start with nodes running on 'node1' and
        # 'node2'
        client = Xapiand(
            ['node1', 'node2'],
            # sniff before doing anything
            sniff_on_start=True,
            # refresh nodes after a node fails to respond
            sniff_on_connection_fail=True,
            # and also every 60 seconds
            sniffer_timeout=60
        )

    Different hosts can have different parameters, use a dictionary per node to
    specify those::

        # connect to localhost directly and another node using port 8881
        # and an url_prefix. Note that ``port`` needs to be an int.
        client = Xapiand([
            {'host': 'localhost'},
            {'host': 'other_host', 'port': 8881, 'url_prefix': 'production'},
        ])

    Alternatively you can use RFC-1738 formatted URLs, as long as they are not
    in conflict with other options::

        client = Xapiand(
            [
                'http://localhost:8880/',
                'https://other_host:8881/production'
            ]
        )

    By default, MsgPackSerializer is used to encode all outgoing requests when
    `msgpack` package is available, otherwise JSONSerializer is used.
    However, you can implement your own custom serializer::

        from xapiand.serializer import MsgPackSerializer

        class SetEncoder(MsgPackSerializer):
            def default(self, obj):
                if isinstance(obj, set):
                    return list(obj)
                if isinstance(obj, Something):
                    return 'CustomSomethingRepresentation'
                return MsgPackSerializer.default(self, obj)

        client = Xapiand(serializer=SetEncoder())

    """
    def __init__(self, hosts=None, transport_class=Transport, **kwargs):
        """
        :arg hosts: list of nodes we should connect to. Node should be a
            dictionary ({"host": "localhost", "port": 8880}), the entire dictionary
            will be passed to the :class:`~xapiand.Connection` class as
            kwargs, or a string in the format of ``host[:port]`` which will be
            translated to a dictionary automatically.  If no value is given the
            :class:`~xapiand.Urllib3HttpConnection` class defaults will be used.

        :arg transport_class: :class:`~xapiand.Transport` subclass to use.

        :arg kwargs: any additional arguments will be passed on to the
            :class:`~xapiand.Transport` class and, subsequently, to the
            :class:`~xapiand.Connection` instances.
        """
        self.transport = transport_class(_normalize_hosts(hosts), **kwargs)

        # namespaced clients for compatibility with API names
        self.indices = IndicesClient(self)

    def __repr__(self):
        try:
            # get a list of all connections
            cons = self.transport.hosts
            # truncate to 5 if there are too many
            if len(cons) > 5:
                cons = cons[:5] + ['...']
            return "<{cls}({cons})>".format(cls=self.__class__.__name__, cons=cons)
        except Exception:
            # probably operating on custom transport and connection_pool, ignore
            return super(Xapiand, self).__repr__()

    @query_params()
    def ping(self, params=None):
        """
        Returns True if the cluster is up, False otherwise.
        """
        try:
            return self.transport.perform_request('HEAD', '/', params=params)
        except TransportError:
            return False

    @query_params('commit', 'timeout')
    def index(self, index, body, id=None, params=None):
        """
        Adds or updates a document in a specific index, making it searchable.

        :arg index: The name of the index
        :arg body: The document
        :arg id: Document ID
        :arg commit: If `true` then commit the document to make this operation
            immediately visible to search, if `false` (the default) then do
            not manually commit anything.
        :arg timeout: Explicit operation timeout
        """
        for param in (index, body):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        method = 'POST' if id in SKIP_IN_PATH else 'PUT'
        return self.transport.perform_request(method, _make_path(index, id),
            params=params, body=body)

    @query_params('selector', 'timeout')
    def update(self, index, id, body=None, params=None):
        """
        Update a document based on a partial data provided.

        :arg index: The name of the index
        :arg id: Document ID
        :arg body: The request definition using a partial `doc`
        :arg selector: A comma-separated list of fields to return in the response
        :arg timeout: Explicit operation timeout
        """
        for param in (index, id):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        return self.transport.perform_request('UPDATE', _make_path(index, id),
            params=params, body=body)

    @query_params('timeout')
    def store(self, index, id, content_type, body=None, params=None):
        """
        Store content in a document.

        :arg index: The name of the index
        :arg id: Document ID
        :arg body: The body to store
        :arg timeout: Explicit operation timeout
        """
        for param in (index, id):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        return self.transport.perform_request('STORE', _make_path(index, id),
            params=params, body=body,
            headers={'content-type': content_type})

    @query_params('refresh', 'routing', 'timeout')
    def exists(self, index, id, params=None):
        """
        Returns a boolean indicating whether or not given document exists in Xapiand.

        :arg index: The name of the index
        :arg id: The document ID
        :arg refresh: Refresh the shard containing the document before
            performing the operation
        :arg timeout: Explicit operation timeout
        """
        for param in (index, id):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        return self.transport.perform_request('HEAD', _make_path(index, id),
            params=params)

    @query_params('selector', 'refresh', 'timeout')
    def get(self, index, id, params=None):
        """
        Get a document from the index based on its id.

        :arg index: The name of the index
        :arg id: The document ID
        :arg selector: A comma-separated list of fields to return in the
            response
        :arg refresh: Refresh the shard containing the document before
            performing the operation
        :arg timeout: Explicit operation timeout
        """
        for param in (index, id):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        return self.transport.perform_request('GET', _make_path(index, id),
            params=params)

    @query_params('timeout')
    def delete(self, index, id, params=None):
        """
        Delete a document from a specific index based on its id.

        :arg index: The name of the index
        :arg id: The document ID
        :arg timeout: Explicit operation timeout
        """
        for param in (index, id):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        return self.transport.perform_request('DELETE', _make_path(index, id),
            params=params)

    @query_params()
    def info(self, index, id=None, params=None):
        """
        Returns information and statistics on an index or a particular document.

        :arg index: The index in which the document resides.
        :arg id: The id of the document, when not specified a doc param should
            be supplied.
        """
        for param in (index,):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        return self.transport.perform_request('GET', _make_path(index,
            ':info', id), params=params)

    @query_params('q', 'refresh', 'timeout')
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
        return self.transport.perform_request('GET', _make_path(index,
            ':count'), params=params, body=body)

    @query_params('q', 'offset', 'limit', 'sort', 'selector', 'refresh', 'timeout')
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
        return self.transport.perform_request('GET', _make_path(index,
            ':search'), params=params, body=body)

    def _restore(self, body, index, params=None):
        for param in (index, body):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")

        content_type, body = self.transport.serializer.nddumps(body)
        return self.transport.perform_request('POST', _make_path(index,
            ':restore'), params=params, body=body, headers={'content-type': content_type})

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
        return streaming_bulk(
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
