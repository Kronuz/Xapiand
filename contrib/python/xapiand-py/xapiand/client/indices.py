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

from .utils import NamespacedClient, query_params, _make_path, SKIP_IN_PATH


class IndicesClient(NamespacedClient):
    @query_params('timeout')
    def commit(self, index=None, params=None):
        """
        Explicitly commit one or more index, making all operations performed
        since the last commit available for search.

        :arg index: A comma-separated list of index names; use `_all` or empty
            string to perform the operation on all indices
        :arg timeout: Explicit operation timeout
        """
        return self.transport.perform_request('POST', _make_path(index,
            ':refresh'), params=params)

    @query_params('timeout')
    def open(self, index, params=None):
        """
        Open a closed index to make it available for search.

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('POST', _make_path(index,
            ':open'), params=params)

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
        return self.transport.perform_request('POST', _make_path(index,
            ':close'), params=params)

    @query_params('timeout')
    def delete(self, index, params=None):
        """
        Delete an index in Xapiand

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('POST', _make_path(index,
            ':delete'), params=params)
