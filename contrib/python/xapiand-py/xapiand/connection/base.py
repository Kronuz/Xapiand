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

import logging

try:
    import simplejson as json
except ImportError:
    import json

try:
    import msgpack
except ImportError:
    msgpack = None

from ..exceptions import TransportError, HTTP_EXCEPTIONS

logger = logging.getLogger('xapiand')

# create the xapiand.trace logger, but only set propagate to False if the
# logger hasn't already been configured
_tracer_already_configured = 'xapiand.trace' in logging.Logger.manager.loggerDict
tracer = logging.getLogger('xapiand.trace')
if not _tracer_already_configured:
    tracer.propagate = False


class Connection(object):
    """
    Class responsible for maintaining a connection to an Xapiand node. It
    holds persistent connection pool to it and it's main interface
    (`perform_request`) is thread-safe.

    Also responsible for logging.
    """
    def __init__(self, host='localhost', port=8880, use_ssl=False, url_prefix='', timeout=10, scheme='http', idx=None, name=None, active=None):
        """
        :arg host: hostname of the node (default: localhost)
        :arg port: port to use (integer, default: 8880)
        :arg url_prefix: optional url prefix for Xapiand
        :arg timeout: default timeout in seconds (float, default: 10)
        """
        self.idx = idx
        self.name = name
        self.active = active
        if use_ssl or scheme == 'https':
            scheme = 'https'
            use_ssl = True
        self.use_ssl = use_ssl

        self.host = '%s://%s:%s' % (scheme, host, port)
        if url_prefix:
            url_prefix = '/' + url_prefix.strip('/')
        self.url_prefix = url_prefix
        self.timeout = timeout

    def __repr__(self):
        if self.name:
            return '<%s: %s (%s:%s)>' % (self.__class__.__name__, self.host, self.idx, self.name)
        return '<%s: %s>' % (self.__class__.__name__, self.host)

    def _pretty_json(self, data, content_type):
        # pretty JSON in tracer curl logs
        try:
            if 'application/json' in content_type:
                data = json.loads(data)
            if 'application/x-ndjson' in content_type:
                data = [json.loads(d) for d in data.split('\n')]
            elif 'application/x-msgpack' in content_type:
                unpacker = msgpack.Unpacker(None)
                unpacker.feed(data)
                data = list(unpacker)
        except Exception:
            return repr(data)
        return json.dumps(data, sort_keys=True, indent=2, separators=(',', ': ')).replace("'", r'\u0027')

    def _log_trace(self, method, path, body, body_content_type, status_code, response, response_content_type, duration):
        if not tracer.isEnabledFor(logging.INFO) or not tracer.handlers:
            return

        # include pretty in trace curls
        path = path.replace("?", "?pretty&", 1) if "?" in path else path + "?pretty"
        tracer.info("curl %s-X%s 'http://localhost:8880%s' -d '%s'",
                    "-H 'Content-Type: application/json' " if body else "",
                    method, path, self._pretty_json(body, body_content_type) if body else "")

        if tracer.isEnabledFor(logging.DEBUG):
            tracer.debug("#[%s] (%.3fs)\n#%s", status_code, duration, self._pretty_json(response, response_content_type).replace('\n', '\n#') if response else '')

    def log_request_success(self, method, full_url, path, body, body_content_type, status_code, response, response_content_type, duration):
        """ Log a successful API call.  """
        #  TODO: optionally pass in params instead of full_url and do urlencode only when needed

        logger.info(
            "%s %s [status:%s request:%.3fs]", method, full_url,
            status_code, duration
        )
        logger.debug('> %s', self._pretty_json(body, body_content_type))
        logger.debug('< %s', self._pretty_json(response, response_content_type))

        self._log_trace(method, path, body, body_content_type, status_code, response, response_content_type, duration)

    def log_request_fail(self, method, full_url, path, body, body_content_type, duration, status_code=None, response=None, response_content_type=None, exception=None):
        """ Log an unsuccessful API call.  """
        # do not log 404s on HEAD requests
        if method == 'HEAD' and status_code == 404:
            return
        logger.warning(
            "%s %s [status:%s request:%.3fs]", method, full_url,
            status_code or "N/A", duration, exc_info=exception is not None
        )

        logger.debug('> %s', self._pretty_json(body, body_content_type))

        self._log_trace(method, path, body, body_content_type, status_code, response, response_content_type, duration)

        if response is not None:
            logger.debug('< %s', self._pretty_json(response, response_content_type))

    def _raise_error(self, status_code, data):
        """ Locate appropriate exception and raise it. """
        error_message = "Unknown error"
        if isinstance(data, dict):
            if 'message' in data:
                error_message = data['message']
            elif 'code' in data:
                error_message = "Error Code: {}".format(data['code'])
            elif 'type' in data:
                error_message = data['type']
            elif 'status' in data:
                error_message = "Status Code: {}".format(data['status'])
        raise HTTP_EXCEPTIONS.get(status_code, TransportError)(status_code, error_message, data)
