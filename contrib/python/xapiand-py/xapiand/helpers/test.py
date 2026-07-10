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
import os
from unittest import IsolatedAsyncioTestCase, SkipTest

from xapiand import Xapiand
from xapiand.exceptions import ConnectionError


async def get_test_client(nowait=False, **kwargs):
    # construct kwargs from the environment
    kw = {'timeout': 30}
    if 'TEST_CONNECTION' in os.environ:
        from xapiand import connection
        kw['connection_class'] = getattr(connection, os.environ['TEST_CONNECTION'])

    kw.update(kwargs)
    client = Xapiand([os.environ.get('TEST_SERVER', {})], **kw)

    # wait for the node to be reachable
    for _ in range(1 if nowait else 100):
        try:
            if await client.ping():
                return client
        except ConnectionError:
            pass
        await asyncio.sleep(.1)

    # timeout
    await client.close()
    raise SkipTest("Xapiand failed to start.")


def _get_version(version_string):
    if '.' not in version_string:
        return ()
    version = version_string.strip().split('.')
    return tuple(int(v) if v.isdigit() else 999 for v in version)


class XapiandTestCase(IsolatedAsyncioTestCase):
    @staticmethod
    async def _get_client():
        return await get_test_client()

    async def asyncSetUp(self):
        self.client = await self._get_client()

    async def asyncTearDown(self):
        await self.client.indexes.delete(index='*', ignore=404)
        await self.client.close()

    async def version(self):
        info = await self.client.info()
        return _get_version(info['version']['number'])
