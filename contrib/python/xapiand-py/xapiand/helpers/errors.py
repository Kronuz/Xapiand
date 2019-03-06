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

from ..exceptions import XapiandException


class BulkIndexError(XapiandException):
    @property
    def errors(self):
        """List of errors from execution of the last chunk."""
        return self.args[1]


class ScanError(XapiandException):
    def __init__(self, scroll_id, *args, **kwargs):
        super(ScanError, self).__init__(*args, **kwargs)
        self.scroll_id = scroll_id
