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
from ..exceptions import NotFoundError
from ..compat import string_types, urlparse, unquote
from .indices import IndicesClient
from .documents import DocumentsClient

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
        self.documents = DocumentsClient(self)
        self.indices = IndicesClient(self)

        # Shortcuts
        self.index = self.documents.index
        self.update = self.documents.update
        self.patch = self.documents.patch
        self.exists = self.documents.exists
        self.get = self.documents.get
        self.delete = self.documents.delete
        self.info = self.documents.info

        self.ping = self.indices.ping
        self.count = self.indices.count
        self.search = self.indices.search
        self.streaming_restore = self.indices.streaming_restore
        self.restore = self.indices.restore
        self.parallel_restore = self.indices.parallel_restore

        self.DoesNotExist = NotFoundError

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
