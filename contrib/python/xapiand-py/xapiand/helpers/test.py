import time
import os
try:
    # python 2.6
    from unittest2 import TestCase, SkipTest
except ImportError:
    from unittest import TestCase, SkipTest

from xapiand import Xapiand
from xapiand.exceptions import ConnectionError


def get_test_client(nowait=False, **kwargs):
    # construct kwargs from the environment
    kw = {'timeout': 30}
    if 'TEST_CONNECTION' in os.environ:
        from xapiand import connection
        kw['connection_class'] = getattr(connection, os.environ['TEST_CONNECTION'])

    kw.update(kwargs)
    client = Xapiand([os.environ.get('TEST_SERVER', {})], **kw)

    # wait for yellow status
    for _ in range(1 if nowait else 100):
        try:
            client.cluster.health(wait_for_status='yellow')
            return client
        except ConnectionError:
            time.sleep(.1)
    else:
        # timeout
        raise SkipTest("Xapiand failed to start.")


def _get_version(version_string):
    if '.' not in version_string:
        return ()
    version = version_string.strip().split('.')
    return tuple(int(v) if v.isdigit() else 999 for v in version)


class XapiandTestCase(TestCase):
    @staticmethod
    def _get_client():
        return get_test_client()

    @classmethod
    def setUpClass(cls):
        super(XapiandTestCase, cls).setUpClass()
        cls.client = cls._get_client()

    def tearDown(self):
        super(XapiandTestCase, self).tearDown()
        self.client.indices.delete(index='*', ignore=404)
        self.client.indices.delete_template(name='*', ignore=404)

    @property
    def version(self):
        if not hasattr(self, '_version'):
            version_string = self.client.info()['version']['number']
            self._version = _get_version(version_string)
        return self._version
