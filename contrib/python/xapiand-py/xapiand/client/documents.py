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

from ..utils import NamespacedClient, query_params, make_url, SKIP_IN_PATH
from ..exceptions import NotFoundError


class DocumentsClient(NamespacedClient):
    @query_params('commit', 'timeout')
    def index(self, index, body, id=None, content_type=None, params=None):
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
        return self.transport.perform_request(method, make_url(index, id=id),
            params=params, body=body,
            headers=content_type and {'content-type': content_type})

    @query_params('selector', 'timeout', 'upsert')
    def update(self, index, id, body=None, content_type=None, params=None):
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
        return self.transport.perform_request('UPDATE', make_url(index, id=id),
            params=params, body=body,
            headers=content_type and {'content-type': content_type})

    @query_params('selector', 'timeout', 'upsert')
    def patch(self, index, id, body=None, params=None):
        """
        Patch a document based on a sequence of operations to apply to a
        JSON document (see RFC 6902.)

        :arg index: The name of the index
        :arg id: Document ID
        :arg body: The request definition using a partial `doc`
        :arg selector: A comma-separated list of fields to return in the response
        :arg timeout: Explicit operation timeout
        """
        for param in (index, id):
            if param in SKIP_IN_PATH:
                raise ValueError("Empty value passed for a required argument.")
        return self.transport.perform_request('PATCH', make_url(index, id=id),
            params=params, body=body)

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
        return self.transport.perform_request('HEAD', make_url(index, id=id),
            params=params)

    @query_params('selector', 'refresh', 'timeout', 'volatile')
    def get(self, index, id, accept=None, params=None):
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
        return self.transport.perform_request('GET', make_url(index, id=id),
            params=params,
            headers=accept and {'accept': accept})

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
        return self.transport.perform_request('DELETE', make_url(index, id=id),
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
        return self.transport.perform_request('INFO', make_url(index, id=id),
            params=params)
