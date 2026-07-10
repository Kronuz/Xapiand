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

import asyncio
import base64
import ssl
import time
import warnings
import zlib
from urllib.parse import urlencode

import aiohttp

from .base import Connection
from ..serializer import DEFAULT_SERIALIZER
from ..exceptions import ConnectionError, ConnectionTimeout, SSLError, ImproperlyConfigured


# sentinel value for `verify_certs`, used to detect whether the user passed a
# value so we can warn when SSL kwargs are combined with an explicit SSLContext.
VERIFY_CERTS_DEFAULT = None

CA_CERTS = None
try:
    import certifi
    CA_CERTS = certifi.where()
except ImportError:
    pass


def create_ssl_context(**kwargs):
    """
    A helper function around creating an SSL context.

    https://docs.python.org/3/library/ssl.html#context-creation

    Accepts kwargs in the same manner as `create_default_context`.
    """
    return ssl.create_default_context(**kwargs)


class AIOHttpConnection(Connection):
    """
    Default connection class using the ``aiohttp`` library and the asyncio
    event loop.

    :arg host: hostname of the node (default: localhost)
    :arg port: port to use (integer, default: 8880)
    :arg timeout: default timeout in seconds (float, default: 10)
    :arg http_auth: optional http auth information as either ':' separated
        string or a tuple
    :arg use_ssl: use ssl for the connection if `True`
    :arg verify_certs: whether to verify SSL certificates
    :arg ca_certs: optional path to CA bundle.
    :arg client_cert: path to the file containing the private key and the
        certificate, or cert only if using client_key
    :arg client_key: path to the file containing the private key if using
        separate cert and key files (client_cert will contain only the cert)
    :arg ssl_version: version of the SSL protocol to use.
    :arg ssl_assert_fingerprint: verify the supplied certificate fingerprint if
        not `None`
    :arg ssl_context: pre-built :class:`ssl.SSLContext` to use instead of the
        individual SSL kwargs
    :arg maxsize: the maximum number of concurrent connections kept open to this
        host (default: 10)
    :arg headers: any custom http headers to be added to requests
    :arg http_compression: `gzip` or `deflate` to activate HTTP compression
    """
    def __init__(self, host='localhost', port=8880, http_auth=None,
                 use_ssl=False, verify_certs=VERIFY_CERTS_DEFAULT, ca_certs=None,
                 client_cert=None, client_key=None, ssl_version=None,
                 ssl_assert_fingerprint=None, maxsize=10, headers=None,
                 ssl_context=None, http_compression=None, **kwargs):
        super().__init__(host=host, port=port, use_ssl=use_ssl, **kwargs)

        self.http_compression = http_compression

        self.headers = {'connection': 'keep-alive'}
        if http_auth is not None:
            if isinstance(http_auth, (tuple, list)):
                http_auth = ':'.join(http_auth)
            credentials = base64.b64encode(http_auth.encode('utf-8')).decode('ascii')
            self.headers['authorization'] = 'Basic %s' % credentials

        # update headers in lowercase to allow overriding of auth headers
        if headers:
            for k in headers:
                self.headers[k.lower()] = headers[k]

        if self.http_compression:
            self.headers['accept-encoding'] = 'gzip,deflate'
            self.headers['content-encoding'] = self.http_compression

        self.headers.setdefault('content-type', DEFAULT_SERIALIZER.mimetype)

        self._ssl_context = None
        if self.use_ssl:
            if ssl_context is not None:
                if (verify_certs is not VERIFY_CERTS_DEFAULT) or ca_certs or client_cert or client_key or ssl_version:
                    warnings.warn("When using `ssl_context`, all other SSL related kwargs are ignored")
                self._ssl_context = ssl_context
            else:
                if verify_certs is VERIFY_CERTS_DEFAULT:
                    verify_certs = True
                ca_certs = CA_CERTS if ca_certs is None else ca_certs
                if verify_certs:
                    if not ca_certs:
                        raise ImproperlyConfigured(
                            "Root certificates are missing for certificate validation. "
                            "Either pass them in using the ca_certs parameter or install "
                            "certifi to use it automatically.")
                    context = ssl.create_default_context(cafile=ca_certs)
                    if client_cert:
                        context.load_cert_chain(client_cert, client_key)
                    self._ssl_context = context
                else:
                    warnings.warn(
                        "Connecting to %s using SSL with verify_certs=False is insecure." % host)
                    context = ssl.create_default_context()
                    context.check_hostname = False
                    context.verify_mode = ssl.CERT_NONE
                    self._ssl_context = context

        self._maxsize = maxsize
        self._session = None
        self._loop = None

    async def _get_session(self):
        loop = asyncio.get_event_loop()
        # recreate the session if it was created on a different (closed) loop
        if self._session is None or self._session.closed or self._loop is not loop:
            if self._session is not None and not self._session.closed:
                await self._session.close()
            connector = aiohttp.TCPConnector(
                limit=self._maxsize,
                ssl=self._ssl_context if self.use_ssl else None,
            )
            self._session = aiohttp.ClientSession(connector=connector)
            self._loop = loop
        return self._session

    async def perform_request(self, method, url, params=None, body=None,
                              timeout=None, ignore=(), headers=None, deserializer=None):
        full_url = url
        if params:
            full_url = '%s?%s' % (full_url, urlencode(params, doseq=True))
        full_url = self.host + full_url

        request_headers = self.headers
        if headers:
            request_headers = request_headers.copy()
            request_headers.update(headers)

        body_content_type = request_headers.get('content-type')

        request_body = body
        if self.http_compression and body:
            compress = zlib.compressobj(
                -1,
                zlib.DEFLATED,
                zlib.MAX_WBITS + (16 if self.http_compression == 'gzip' else 0),
                zlib.DEF_MEM_LEVEL,
                zlib.Z_DEFAULT_STRATEGY,
            )
            request_body = compress.compress(body) + compress.flush()

        request_timeout = aiohttp.ClientTimeout(total=timeout or self.timeout)

        start = time.time()
        try:
            session = await self._get_session()
            async with session.request(
                method,
                full_url,
                data=request_body,
                headers=request_headers,
                timeout=request_timeout,
            ) as response:
                raw_data = await response.read()
                duration = time.time() - start
                status = response.status
                response_headers = {k.lower(): v for k, v in response.headers.items()}
        except Exception as e:
            self.log_request_fail(method, full_url, full_url, body, body_content_type, time.time() - start, exception=e)
            if isinstance(e, (ssl.SSLError, aiohttp.ClientSSLError)):
                raise SSLError("N/A", str(e), e)
            if isinstance(e, asyncio.TimeoutError):
                raise ConnectionTimeout("TIMEOUT", str(e), e)
            raise ConnectionError("N/A", str(e), e)

        data_content_type = response_headers.get('content-type')
        data = deserializer.loads(raw_data, data_content_type) if raw_data and deserializer else raw_data

        # raise errors based on http status codes, let the client handle those if needed
        if not (200 <= status < 300) and status not in ignore:
            self.log_request_fail(method, full_url, full_url, body, body_content_type, duration, status, raw_data, data_content_type)
            self._raise_error(status, data)

        self.log_request_success(method, full_url, full_url, body, body_content_type, status, raw_data, data_content_type, duration)

        return status, response_headers, data

    async def close(self):
        """
        Explicitly close the underlying aiohttp session.
        """
        if self._session is not None and not self._session.closed:
            await self._session.close()
            self._session = None
