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
import warnings
try:
    import requests
    REQUESTS_AVAILABLE = True
except ImportError:
    REQUESTS_AVAILABLE = False

from .base import Connection
from ..exceptions import ConnectionError, ImproperlyConfigured, ConnectionTimeout, SSLError
from ..compat import urlencode, string_types
from ..serializer import DEFAULT_SERIALIZER


class RequestsHttpConnection(Connection):
    """
    Connection using the `requests` library.

    :arg http_auth: optional http auth information as either ':' separated
        string or a tuple. Any value will be passed into requests as `auth`.
    :arg use_ssl: use ssl for the connection if `True`
    :arg verify_certs: whether to verify SSL certificates
    :arg ca_certs: optional path to CA bundle. By default standard requests'
        bundle will be used.
    :arg client_cert: path to the file containing the private key and the
        certificate, or cert only if using client_key
    :arg client_key: path to the file containing the private key if using
        separate cert and key files (client_cert will contain only the cert)
    :arg headers: any custom http headers to be add to requests
    """
    def __init__(self, host='localhost', port=8880, http_auth=None,
                 use_ssl=False, verify_certs=True, ca_certs=None, client_cert=None,
                 client_key=None, headers=None, **kwargs):
        if not REQUESTS_AVAILABLE:
            raise ImproperlyConfigured("Please install requests to use RequestsHttpConnection.")

        super(RequestsHttpConnection, self).__init__(host=host, port=port, use_ssl=use_ssl, **kwargs)
        self.session = requests.Session()
        self.session.headers = headers or {}
        self.session.headers.setdefault('content-type', DEFAULT_SERIALIZER.mimetype)
        if http_auth is not None:
            if isinstance(http_auth, (tuple, list)):
                http_auth = tuple(http_auth)
            elif isinstance(http_auth, string_types):
                http_auth = tuple(http_auth.split(':', 1))
            self.session.auth = http_auth
        self.base_url = 'http%s://%s:%d%s' % ('s' if self.use_ssl else '', host, port, self.url_prefix)
        self.session.verify = verify_certs
        if not client_key:
            self.session.cert = client_cert
        elif client_cert:
            # cert is a tuple of (certfile, keyfile)
            self.session.cert = (client_cert, client_key)
        if ca_certs:
            if not verify_certs:
                raise ImproperlyConfigured("You cannot pass CA certificates when verify SSL is off.")
            self.session.verify = ca_certs

        if self.use_ssl and not verify_certs:
            warnings.warn(
                "Connecting to %s using SSL with verify_certs=False is insecure." % self.base_url)

    def perform_request(self, method, url, params=None, body=None, timeout=None, ignore=(), headers=None, deserializer=None):
        full_url = self.base_url + url
        if params:
            full_url = '%s?%s' % (full_url, urlencode(params, doseq=True))

        body_content_type = headers.get('content-type')
        start = time.time()
        request = requests.Request(method=method, headers=headers, url=full_url, data=body)
        prepared_request = self.session.prepare_request(request)
        settings = self.session.merge_environment_settings(prepared_request.url, {}, None, None, None)
        send_kwargs = {'timeout': timeout or self.timeout}
        send_kwargs.update(settings)
        try:
            response = self.session.send(prepared_request, **send_kwargs)
            duration = time.time() - start
            status = response.status_code
            headers = response.headers
            raw_data = response.text
            data_content_type = headers.get('content-type')
            data = deserializer.loads(raw_data, data_content_type) if raw_data and deserializer else raw_data
        except Exception as e:
            self.log_request_fail(method, full_url, prepared_request.path_url, body, body_content_type, time.time() - start, exception=e)
            if isinstance(e, requests.exceptions.SSLError):
                raise SSLError("N/A", str(e), e)
            if isinstance(e, requests.Timeout):
                raise ConnectionTimeout("TIMEOUT", str(e), e)
            raise ConnectionError("N/A", str(e), e)

        # raise errors based on http status codes, let the client handle those if needed
        if not (200 <= status < 300) and status not in ignore:
            self.log_request_fail(method, full_url, response.request.path_url, body, body_content_type, duration, status, raw_data, data_content_type)
            self._raise_error(status, data)

        self.log_request_success(method, full_url, response.request.path_url, body, body_content_type, status, raw_data, data_content_type, duration)

        return status, headers, data

    def close(self):
        """
        Explicitly closes connections
        """
        self.session.close()
