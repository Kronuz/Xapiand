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

import time

from ..exceptions import TransportError
from ..compat import zip, Queue

from .errors import BulkIndexError

import logging


logger = logging.getLogger('xapiand.helpers')


def chunk_objects(objects, chunk_size, max_chunk_bytes, serializer):
    """
    Split objects into chunks by number or size, serialize them into strings in
    the process.
    """
    bulk_objs, bulk_ser_objs = [], []
    size, count = 0, 0
    for obj in objects:
        ser_obj = serializer.dumps(obj)
        cur_size = len(ser_obj) + 1

        # full chunk, send it and start a new one
        if size + cur_size > max_chunk_bytes or count == chunk_size:
            yield bulk_objs, bulk_ser_objs
            bulk_objs, bulk_ser_objs = [], []
            size, count = 0, 0

        bulk_objs.append(obj)
        bulk_ser_objs.append(ser_obj)

        size += cur_size
        count += 1

    if bulk_objs:
        yield bulk_objs, bulk_ser_objs


def process_bulk_chunk(processor, bulk_objs, bulk_ser_objs,
                       raise_on_exception=True, raise_on_error=True,
                       *args, **kwargs):
    """
    Send a bulk request to xapiand and process the output.
    """

    try:
        # send the actual request
        resp = processor(bulk_ser_objs, *args, **kwargs)
    except TransportError as e:
        # default behavior - just propagate exception
        if raise_on_exception:
            raise e

        # if we are not propagating, mark all objects in current chunk as failed
        err_message = str(e)
        exc_errors = []

        for obj in bulk_objs:
            # collect all the information about failed objects
            info = {
                '_status': e.status_code,
                '_message': [err_message],
                '_exception': e,
            }
            info.update(obj)
            exc_errors.append(info)

        # emulate standard behavior for failed objects
        if raise_on_error:
            raise BulkIndexError("%d document(s) failed to index." % len(exc_errors), exc_errors)
        else:
            status = e.status_code
            for err in exc_errors:
                yield status, err
            return

    # if raise on error is set, we need to collect errors per chunk before raising them
    errors = []

    # go through request-response pairs and detect failures
    for obj, item in zip(bulk_objs, resp['items']):
        status = item.get('_status', 200)
        # include original document source
        item.update(obj)
        if 200 <= status < 300 or not raise_on_error:
            # if we are not just recording all errors to be able to raise
            # them all at once, yield items individually
            yield status, item
        else:
            errors.append(item)

    if errors:
        raise BulkIndexError("%d document(s) failed to index." % len(errors), errors)


def streaming_bulk(processor, objects, serializer,
                   chunk_size=500, max_chunk_bytes=100 * 1024 * 1024,
                   raise_on_error=True, raise_on_exception=True, max_retries=0,
                   initial_backoff=2, max_backoff=600, yield_ok=True, *args, **kwargs):
    """
    Streaming bulk consumes objects from the iterable passed in and yields
    results per object. For non-streaming usecases use
    :func:`~xapiand.helpers.bulk` which is a wrapper around streaming
    bulk that returns summary information about the bulk operation once the
    entire input is consumed and sent.

    If you specify ``max_retries`` it will also retry any documents that were
    rejected with a ``429`` status code. To do this it will wait (**by calling
    time.sleep which will block**) for ``initial_backoff`` seconds and then,
    every subsequent rejection for the same chunk, for double the time every
    time up to ``max_backoff`` seconds.

    :arg processor: function to process the objects chunks
    :arg objects: iterable containing the objects to be processed
    :arg serializer: serializer instance
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
    """
    for bulk_objs, bulk_ser_objs in chunk_objects(objects, chunk_size,
                                                  max_chunk_bytes,
                                                  serializer):

        for attempt in range(max_retries + 1):
            to_retry_objs, to_retry_ser_objs = [], []
            if attempt:
                time.sleep(min(max_backoff, initial_backoff * 2**(attempt - 1)))

            try:
                for obj, ser_obj, (status, info) in zip(
                    bulk_objs,
                    bulk_ser_objs,
                    process_bulk_chunk(
                        processor,
                        bulk_objs, bulk_ser_objs,
                        raise_on_exception,
                        raise_on_error,
                        *args, **kwargs
                    )
                ):

                    if 200 <= status < 300:
                        if yield_ok:
                            yield status, info
                    else:
                        # retry if retries enabled, we get 429, and we are not
                        # in the last attempt
                        if max_retries and status == 429 and attempt + 1 <= max_retries:
                            to_retry_objs.append(obj)
                            to_retry_ser_objs.append(ser_obj)
                        else:
                            yield status, info

            except TransportError as e:
                # suppress 429 errors since we will retry them
                if attempt == max_retries or e.status_code != 429:
                    raise
            else:
                if not to_retry_ser_objs:
                    break
                # retry only subset of documents that didn't succeed
                bulk_objs, bulk_ser_objs = to_retry_objs, to_retry_ser_objs


def bulk(processor, objects, serializer, stats_only=False, *args, **kwargs):
    """
    Helper for the :meth:`~xapiand.Xapiand.bulk` api that provides
    a more human friendly interface - it consumes an iterator of objects and
    sends them to xapiand in chunks. It returns a tuple with summary
    information - number of successfully processed objects and either list of
    errors or number of errors if ``stats_only`` is set to ``True``. Note that
    by default we raise a ``BulkIndexError`` when we encounter an error so
    options like ``stats_only`` only apply when ``raise_on_error`` is set to
    ``False``.

    When errors are being collected original document data is included in the
    error dictionary which can lead to an extra high memory usage. If you need
    to process a lot of data and want to ignore/collect errors please consider
    using the :func:`~xapiand.helpers.streaming_bulk` helper which will
    just return the errors and not store them in memory.

    :arg processor: function to process the objects chunks
    :arg objects: iterable containing the objects to be processed
    :arg serializer: serializer instance
    :arg stats_only: if `True` only report number of successful/failed
        operations instead of just number of successful and a list of error responses

    Any additional keyword arguments will be passed to
    :func:`~xapiand.helpers.streaming_bulk` which is used to execute
    the operation, see :func:`~xapiand.helpers.streaming_bulk` for more
    accepted parameters.
    """
    success, failed = 0, 0

    # list of errors to be collected is not stats_only
    errors = []

    # make streaming_bulk yield successful results so we can count them
    kwargs['yield_ok'] = True
    for status, item in streaming_bulk(processor, objects, serializer, *args, **kwargs):
        # go through request-response pairs and detect failures
        if 200 <= status < 300:
            success += 1
        else:
            if not stats_only:
                errors.append(item)
            failed += 1

    return success, failed if stats_only else errors


def parallel_bulk(processor, objects, serializer,
                  chunk_size=500, max_chunk_bytes=100 * 1024 * 1024,
                  thread_count=4, queue_size=4,
                  *args, **kwargs):
    """
    Parallel version of the bulk helper run in multiple threads at once.

    :arg processor: function to process the objects chunks
    :arg objects: iterable containing the objects to be processed
    :arg serializer: serializer instance
    :arg thread_count: size of the threadpool to use for the bulk requests
    :arg chunk_size: number of docs in one chunk sent to the server (default: 500)
    :arg max_chunk_bytes: the maximum size of the request in bytes (default: 100MB)
    :arg raise_on_error: raise ``BulkIndexError`` containing errors (as `.errors`)
        from the execution of the last chunk when some occur. By default we raise.
    :arg raise_on_exception: if ``False`` then don't propagate exceptions from
        call to ``bulk`` and just report the items that failed as failed.
    :arg queue_size: size of the task queue between the main thread (producing
        chunks to send) and the processing threads.
    """

    # Avoid importing multiprocessing unless parallel_bulk is used
    # to avoid exceptions on restricted environments like App Engine
    from multiprocessing.pool import ThreadPool

    class BlockingPool(ThreadPool):
        def _setup_queues(self):
            super(BlockingPool, self)._setup_queues()
            self._inqueue = Queue(queue_size)
            self._quick_put = self._inqueue.put

    pool = BlockingPool(thread_count)

    try:
        for result in pool.imap(
            lambda bulk_chunk: list(process_bulk_chunk(processor, bulk_chunk[0], bulk_chunk[1], *args, **kwargs)),
            chunk_objects(objects, chunk_size, max_chunk_bytes, serializer)
        ):
            for item in result:
                yield item

    finally:
        pool.close()
        pool.join()
