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

import weakref
from datetime import date, datetime
from functools import wraps
from ..compat import string_types, quote_plus, PY2


# parts of URL to be omitted
SKIP_IN_PATH = (None, '', b'', [], ())


def _escape(value):
    """
    Escape a single value of a URL string or a query parameter. If it is a list
    or tuple, turn it into a comma-separated string first.
    """

    # make sequences into comma-separated stings
    if isinstance(value, (list, tuple)):
        value = ','.join(value)

    # dates and datetimes into isoformat
    elif isinstance(value, (date, datetime)):
        value = value.isoformat()

    # make bools into true/false strings
    elif isinstance(value, bool):
        value = str(value).lower()

    # don't decode bytestrings
    elif isinstance(value, bytes):
        return value

    # encode strings to utf-8
    if isinstance(value, string_types):
        if PY2 and isinstance(value, unicode):
            return value.encode('utf-8')
        if not PY2 and isinstance(value, str):
            return value.encode('utf-8')

    return str(value)


def make_url(url, id=""):
    """
    Create a normalized URL string.
    """
    if isinstance(url, tuple):
        url = list(url)
    elif not isinstance(url, list):
        url = url.split('/')
    # preserve ',', '*' and '~' in url for nicer URLs in logs
    url = [quote_plus(_escape(u), b',*~') for u in url if u not in SKIP_IN_PATH]
    url.append(quote_plus(_escape(id), b',*~'))
    return '/' + '/'.join(url)


# parameters that apply to all methods
GLOBAL_PARAMS = ('pretty', 'human', 'routing')


def query_params(*accepted_params):
    """
    Decorator that pops all accepted parameters from method's kwargs and puts
    them in the params argument.
    """
    def _wrapper(func):
        @wraps(func)
        def _wrapped(*args, **kwargs):
            params = {}
            if 'params' in kwargs:
                params = kwargs.pop('params').copy()
            for p in accepted_params + GLOBAL_PARAMS:
                if p in kwargs:
                    value = kwargs.pop(p)
                    if value is not None:
                        if isinstance(value, (list, tuple)):
                            params[p] = [_escape(v) for v in value]
                        else:
                            params[p] = _escape(value)

            # don't treat ignore and request_timeout as other params to avoid escaping
            for p in ('ignore', 'request_timeout'):
                if p in kwargs:
                    params[p] = kwargs.pop(p)
            return func(*args, params=params, **kwargs)
        return _wrapped
    return _wrapper


class NamespacedClient(object):
    def __init__(self, client):
        self.client = client

    @property
    def transport(self):
        return self.client.transport


class AddonClient(NamespacedClient):
    @classmethod
    def infect_client(cls, client):
        addon = cls(weakref.proxy(client))
        setattr(client, cls.namespace, addon)
        return client
