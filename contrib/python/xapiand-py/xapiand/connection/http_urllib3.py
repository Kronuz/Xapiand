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
import ssl
import urllib3
from urllib3.exceptions import ReadTimeoutError, SSLError as UrllibSSLError
from urllib3.util.retry import Retry
import warnings
import zlib

from ..serializer import DEFAULT_SERIALIZER


# sentinal value for `verify_certs`.
# This is used to detect if a user is passing in a value for `verify_certs`
# so we can raise a warning if using SSL kwargs AND SSLContext.
VERIFY_CERTS_DEFAULT = None

CA_CERTS = None

try:
    import certifi
    CA_CERTS = certifi.where()
except ImportError:
    pass

from .base import Connection
from ..exceptions import ConnectionError, ImproperlyConfigured, ConnectionTimeout, SSLError
from ..compat import urlencode


def create_ssl_context(**kwargs):
    """
    A helper function around creating an SSL context

    https://docs.python.org/3/library/ssl.html#context-creation

    Accepts kwargs in the same manner as `create_default_context`.
    """
    ctx = ssl.create_default_context(**kwargs)
    return ctx


class Urllib3HttpConnection(Connection):
    """
    Default connection class using the `urllib3` library and the http protocol.

    :arg host: hostname of the node (default: localhost)
    :arg port: port to use (integer, default: 8880)
    :arg url_prefix: optional url prefix
    :arg timeout: default timeout in seconds (float, default: 10)
    :arg http_auth: optional http auth information as either ':' separated
        string or a tuple
    :arg use_ssl: use ssl for the connection if `True`
    :arg verify_certs: whether to verify SSL certificates
    :arg ca_certs: optional path to CA bundle.
        See https://urllib3.readthedocs.io/en/latest/security.html#using-certifi-with-urllib3
        for instructions how to get default set
    :arg client_cert: path to the file containing the private key and the
        certificate, or cert only if using client_key
    :arg client_key: path to the file containing the private key if using
        separate cert and key files (client_cert will contain only the cert)
    :arg ssl_version: version of the SSL protocol to use. Choices are:
        SSLv23 (default) SSLv2 SSLv3 TLSv1 (see ``PROTOCOL_*`` constants in the
        ``ssl`` module for exact options for your environment).
    :arg ssl_assert_hostname: use hostname verification if not `False`
    :arg ssl_assert_fingerprint: verify the supplied certificate fingerprint if not `None`
    :arg maxsize: the number of connections which will be kept open to this
        host. See https://urllib3.readthedocs.io/en/1.4/pools.html#api for more
        information.
    :arg headers: any custom http headers to be add to requests
    :arg http_compression: `gzip` or `defalte` activate HTTP compression
    """
    def __init__(self, host='localhost', port=8880, http_auth=None,
            use_ssl=False, verify_certs=VERIFY_CERTS_DEFAULT, ca_certs=None, client_cert=None,
            client_key=None, ssl_version=None, ssl_assert_hostname=None,
            ssl_assert_fingerprint=None, maxsize=10, headers=None, ssl_context=None, http_compression=None, **kwargs):

        super(Urllib3HttpConnection, self).__init__(host=host, port=port, use_ssl=use_ssl, **kwargs)
        self.http_compression = http_compression
        self.headers = urllib3.make_headers(keep_alive=True)
        if http_auth is not None:
            if isinstance(http_auth, (tuple, list)):
                http_auth = ':'.join(http_auth)
            self.headers.update(urllib3.make_headers(basic_auth=http_auth))

        # update headers in lowercase to allow overriding of auth headers
        if headers:
            for k in headers:
                self.headers[k.lower()] = headers[k]

        if self.http_compression:
            self.headers.update(urllib3.make_headers(accept_encoding=True))
            self.headers.update({'content-encoding': self.http_compression})

        self.headers.setdefault('content-type', DEFAULT_SERIALIZER.mimetype)
        pool_class = urllib3.HTTPConnectionPool
        kw = {}

        # if providing an SSL context, raise error if any other SSL related flag is used
        if ssl_context and ((verify_certs is not VERIFY_CERTS_DEFAULT) or ca_certs or client_cert or client_key or ssl_version):
            warnings.warn("When using `ssl_context`, all other SSL related kwargs are ignored")

        # if ssl_context provided use SSL by default
        if ssl_context and self.use_ssl:
            pool_class = urllib3.HTTPSConnectionPool
            kw.update({
                'assert_fingerprint': ssl_assert_fingerprint,
                'ssl_context': ssl_context,
            })

        elif self.use_ssl:
            pool_class = urllib3.HTTPSConnectionPool
            kw.update({
                'ssl_version': ssl_version,
                'assert_hostname': ssl_assert_hostname,
                'assert_fingerprint': ssl_assert_fingerprint,
            })

            # If `verify_certs` is sentinal value, default `verify_certs` to `True`
            if verify_certs is VERIFY_CERTS_DEFAULT:
                verify_certs = True

            ca_certs = CA_CERTS if ca_certs is None else ca_certs
            if verify_certs:
                if not ca_certs:
                    raise ImproperlyConfigured(
                        "Root certificates are missing for certificate validation."
                        "Either pass them in using the ca_certs parameter or install"
                        "certifi to use it automatically.")

                kw.update({
                    'cert_reqs': 'CERT_REQUIRED',
                    'ca_certs': ca_certs,
                    'cert_file': client_cert,
                    'key_file': client_key,
                })
            else:
                warnings.warn(
                    "Connecting to %s using SSL with verify_certs=False is insecure." % host)

        self.pool = pool_class(host, port=port, timeout=self.timeout, maxsize=maxsize, **kw)

    def perform_request(self, method, url, params=None, body=None, timeout=None, ignore=(), headers=None, deserializer=None):
        full_url = self.url_prefix + url
        if params:
            full_url = '%s?%s' % (full_url, urlencode(params, doseq=True))
        full_url = self.host + full_url

        start = time.time()
        try:
            kw = {}
            if timeout:
                kw['timeout'] = timeout

            # in python2 we need to make sure the url and method are not
            # unicode. Otherwise the body will be decoded into unicode too and
            # that will fail (#133, #201).
            if not isinstance(full_url, str):
                full_url = full_url.encode('utf-8')
            if not isinstance(method, str):
                method = method.encode('utf-8')

            request_headers = self.headers
            if headers:
                request_headers = request_headers.copy()
                request_headers.update(headers)
            if self.http_compression and body:
                compress = zlib.compressobj(
                    -1,
                    zlib.DEFLATED,
                    zlib.MAX_WBITS + (16 if self.http_compression == 'gzip' else 0),
                    zlib.DEF_MEM_LEVEL,
                    zlib.Z_DEFAULT_STRATEGY
                )
                compressed_body = compress.compress(body) + compress.flush()
            else:
                compressed_body = body

            body_content_type = request_headers.get('content-type')
            response = self.pool.urlopen(method, full_url, compressed_body, retries=Retry(False), headers=request_headers, **kw)
            duration = time.time() - start
            status = response.status
            headers = response.getheaders()
            raw_data = response.data
            data_content_type = headers.get('content-type')
            data = deserializer.loads(raw_data, data_content_type) if raw_data and deserializer else raw_data
        except Exception as e:
            self.log_request_fail(method, full_url, full_url, body, body_content_type, time.time() - start, exception=e)
            if isinstance(e, UrllibSSLError):
                raise SSLError("N/A", str(e), e)
            if isinstance(e, ReadTimeoutError):
                raise ConnectionTimeout("TIMEOUT", str(e), e)
            raise ConnectionError("N/A", str(e), e)

        # raise errors based on http status codes, let the client handle those if needed
        if not (200 <= status < 300) and status not in ignore:
            self.log_request_fail(method, full_url, full_url, body, body_content_type, duration, status, raw_data, data_content_type)
            self._raise_error(status, data)

        self.log_request_success(method, full_url, full_url, body, body_content_type, status, raw_data, data_content_type, duration)

        return status, headers, data

    def close(self):
        """
        Explicitly closes connection
        """
        self.pool.close()
