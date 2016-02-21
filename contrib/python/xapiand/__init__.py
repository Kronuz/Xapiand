# -*- coding: utf-8 -*-
#
# Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
from __future__ import absolute_import

import json
import os

try:
    import requests
except ImportError:
    raise ImportError("Xapiand requires the installation of the requests module.")


class XapiandResponse(object):
    def __init__(self, response, size=0, result=None, results=None, facets=None, results_stream=None, facets_stream=None):
        self._response = response

        self.size = size

        self.value = self.result = result

        self.results = results
        self.results_stream = results_stream
        try:
            self.value = self._results_first = self.next('results', False)
        except StopIteration:
            pass

        self.facets = facets
        self.facets_stream = facets_stream
        try:
            self._facets_first = self.next('facets', False)
        except StopIteration:
            pass

    def __len__(self):
        return self.size

    def __iter__(self):
        return self

    def next(self, attr='results', chk_idx=True):
        idx = getattr(self, '_%s_idx', 0)
        if chk_idx and idx >= self.size:
            raise StopIteration

        first = getattr(self, '_%s_first' % attr, None)
        if first is not None:
            setattr(self, '_%s_first' % attr, None)
            setattr(self, '_%s_idx', idx + 1)
            return first

        stream = getattr(self, '%s_stream' % attr)
        if stream is not None:
            res = self.post_process(stream.next())
            setattr(self, '_%s_idx', idx + 1)
            return res

        results = getattr(self, attr)
        if results:
            results = getattr(self, attr)
            setattr(self, '_%s_idx', idx + 1)
            try:
                res = self.post_process(results[idx])
                setattr(self, '_%s_idx', idx + 1)
                return res
            except IndexError:
                pass

        raise StopIteration
    __next__ = next

    def __del__(self):
        if self.results_stream:
            self.results_stream.close()
        if self.facets_stream:
            self.facets_stream.close()

    def post_process(self, result):
        return result


class Xapiand(object):

    """
    An object which manages connections to xapiand and acts as a
    go-between for API calls to it
    """

    session = requests.Session()
    _methods = dict(
        search=(session.get, True, 'results'),
        facets=(session.get, True, 'facets'),
        stats=(session.get, False, 'result'),
        get=(session.get, False, 'result'),
        delete=(session.delete, False, 'result'),
        head=(session.head, False, 'result'),
        index=(session.put, False, 'result'),
        patch=(session.patch, False, 'result'),
    )

    def __init__(self, ip='127.0.0.1', port=8880, commit=False):
        if ip and ':' in ip:
            ip, _, port = ip.partition(':')
        self.ip = ip
        self.port = port
        self.commit = commit

    def build_url(self, action_request, index, doc_type, ip, port, nodename, id, body):
        if ip and ':' in ip:
            ip, _, port = ip.partition(':')
        if not ip:
            ip = self.ip
        if not port:
            port = self.port
        host = '%s:%s' % (ip, port)

        if isinstance(index, (tuple, list)):
            index = ','.join(index)

        if doc_type:
            index += '/%s' % doc_type

        if id is not None:
            if nodename:
                url = 'http://%s/%s@%s/%s' % (host, index, nodename, id)
            else:
                url = 'http://%s/%s/%s' % (host, index, id)
        else:
            if nodename:
                url = 'http://%s/%s@%s/_%s/' % (host, index, nodename, action_request)
            else:
                url = 'http://%s/%s/_%s/' % (host, index, action_request)
        return url

    def _send_request(self, action_request, index, doc_type=None, ip=None, port=None, nodename=None, id=None, body=None, **kwargs):
        """
        :arg action_request: Perform index, delete, serch, facets, stats, patch, head actions per request
        :arg query: Query to process on xapiand
        :arg index: index path
        :arg ip: address to connect to xapiand
        :arg port: port to conntct to xapiand
        :arg nodename: Node name, if empty is assigned randomly
        :arg id: Document ID
        :arg body: File or dictionary with the body of the request
        """
        method, stream, key = self._methods[action_request]

        url = self.build_url(action_request, index, doc_type, ip, port, nodename, id, body)

        params = kwargs.pop('params', None)
        if params is not None:
            kwargs['params'] = dict((k.replace('__', '.'), (v and 1 or 0) if isinstance(v, bool) else v) for k, v in params.items())

        stream = kwargs.pop('stream', stream)
        if stream is not None:
            kwargs['stream'] = stream

        if body is not None:
            if isinstance(body, dict):
                body = json.dumps(body)
            elif os.path.isfile(body):
                body = open(body, 'r')
            res = method(url, body, **kwargs)
        else:
            res = method(url, **kwargs)
        res.raise_for_status()

        is_json = 'application/json' in res.headers['content-type']

        if stream:
            def results(lines):
                for line in lines:
                    # filter out keep-alive new lines
                    if line:
                        yield json.loads(line) if is_json else line
            results = results(res.iter_lines())
        else:
            results = [res.json() if is_json else res.content]

        if key == 'result':
            for result in results:
                results = result
                break

        kwargs = {}
        if stream:
            kwargs['%s_stream' % key] = results
        else:
            kwargs['%s' % key] = results

        kwargs['size'] = int(res.headers.get('X-Matched-count', 0))

        return XapiandResponse(res, **kwargs)

    def search(self, index, doc_type=None, query=None, partial=None, terms=None, offset=None, limit=None, sort=None, facets=None, language=None, pretty=False, kwargs=None, **kw):
        kwargs = kwargs or {}
        kwargs.update(kw)
        kwargs['params'] = dict(
            pretty=pretty,
        )
        if query is None:
            kwargs['params']['query'] = query
        if partial is None:
            kwargs['params']['partial'] = partial
        if terms is None:
            kwargs['params']['terms'] = terms
        if offset is None:
            kwargs['params']['offset'] = offset
        if limit is None:
            kwargs['params']['limit'] = limit
        if sort is None:
            kwargs['params']['sort'] = sort
        if facets is None:
            kwargs['params']['facets'] = facets
        if language is None:
            kwargs['params']['language'] = language
        return self._send_request('search', index, doc_type, **kwargs)

    def facets(self, index, doc_type=None, query=None, partial=None, terms=None, offset=None, limit=None, sort=None, facets=None, language=None, pretty=False, kwargs=None, **kw):
        kwargs = kwargs or {}
        kwargs.update(kw)
        kwargs['params'] = dict(
            pretty=pretty,
        )
        if query is None:
            kwargs['params']['query'] = query
        if partial is None:
            kwargs['params']['partial'] = partial
        if terms is None:
            kwargs['params']['terms'] = terms
        if offset is None:
            kwargs['params']['offset'] = offset
        if limit is None:
            kwargs['params']['limit'] = limit
        if sort is None:
            kwargs['params']['sort'] = sort
        if facets is None:
            kwargs['params']['facets'] = facets
        if language is None:
            kwargs['params']['language'] = language
        return self._send_request('facets', index, doc_type, **kwargs)

    def stats(self, index, doc_type=None, pretty=False, kwargs=None):
        kwargs = kwargs or {}
        kwargs['params'] = dict(
            pretty=pretty,
        )
        return self._send_request('stats', index, doc_type, **kwargs)

    def head(self, index, doc_type, id, pretty=False, kwargs=None):
        kwargs = kwargs or {}
        kwargs['id'] = id
        kwargs['params'] = dict(
            pretty=pretty,
        )
        return self._send_request('head', index, doc_type, **kwargs)

    def get(self, index, doc_type, id, pretty=False, kwargs=None):
        kwargs = kwargs or {}
        kwargs['id'] = id
        kwargs['params'] = dict(
            pretty=pretty,
        )
        return self._send_request('get', index, doc_type, **kwargs)

    def delete(self, index, doc_type, id, commit=None, pretty=False, kwargs=None):
        kwargs = kwargs or {}
        kwargs['id'] = id
        kwargs['params'] = dict(
            commit=self.commit if commit is None else commit,
            pretty=pretty,
        )
        return self._send_request('delete', index, doc_type, **kwargs)

    def index(self, index, doc_type, body, id, commit=None, pretty=False, kwargs=None):
        kwargs = kwargs or {}
        kwargs['id'] = id
        kwargs['body'] = body
        kwargs['params'] = dict(
            commit=self.commit if commit is None else commit,
            pretty=pretty,
        )
        return self._send_request('index', index, doc_type, **kwargs)

    def patch(self, index, doc_type, body, id, commit=None, pretty=False, kwargs=None):
        kwargs = kwargs or {}
        kwargs['id'] = id
        kwargs['body'] = body
        kwargs['params'] = dict(
            commit=self.commit if commit is None else commit,
            pretty=pretty,
        )
        return self._send_request('patch', index, doc_type, **kwargs)
