# -*- coding: utf-8 -*-
#
# Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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


class Xapiand(object):

    """
    An object which manages connections to xapiand and acts as a
    go-between for API calls to it
    """

    def __init__(self):
        self.port = '8880'
        self.method = dict()
        self.method['search'] = requests.get
        self.method['facets'] = requests.get
        self.method['stats'] = requests.get
        self.method['delete'] = requests.delete
        self.method['head'] = requests.head
        self.method['index'] = requests.put
        self.method['patch'] = requests.patch
        self.has_3arg = {'search': False, 'facets': False, 'stats': False, 'head': False, 'delete': False, 'index': True, 'patch': True}
        self.has_id = {'search': False, 'facets': False, 'stats': False, 'delete': True, 'head': True, 'index': True, 'patch': True}

    def build_url(self, action_request, query, endpoint, ip, body, _id, nodename):
        if type(endpoint) is list:
            endpoint = ','.join(endpoint)

        if ':' not in ip:
            ip = ip + ':' + self.port

        if self.has_id[action_request] or (action_request == 'search' and _id is not None):
            if nodename:
                url = 'http://' + ip + '/' + endpoint + '@' + nodename + '/' + _id
            else:
                url = 'http://' + ip + '/' + endpoint + '/' + _id
            if query:
                url += '/?' + query
        else:
            if nodename:
                url = 'http://' + ip + '/' + endpoint + '@' + nodename + '/_' + action_request + '/?' + query
            else:
                url = 'http://' + ip + '/' + endpoint + '/_' + action_request + '/?' + query
        return url

    def send_request(self, action_request, endpoint, query=None, ip='127.0.0.1', body=None, _id=None, nodename=None, time_out=None):

        """
        :arg action_request: Perform  index, delete, serch, facets, stats, patch, head actions per request
        :arg query: Query to process on xapiand
        :arg endpoint: index path
        :arg ip: address to connect to xapiand
        :arg body: File or dictionary with the body of the request
        :arg _id: Document ID
        :arg nodename: Node name, if empty is assigned randomly
        """

        response = {}
        url = self.build_url(action_request, query, endpoint, ip, body, _id, nodename)
        try:
            if self.has_3arg[action_request]:
                if isinstance(body, dict):
                    res = self.method[action_request](url, json.dumps(body), timeout=time_out)
                    response['results'] = res.iter_lines()
                elif os.path.isfile(body):
                    res = self.method[action_request](url, open(body, 'r'), timeout=time_out)
                    response['results'] = res.iter_lines()
            else:
                res = self.method[action_request](url, timeout=time_out)
                if action_request == 'facets':
                    response['facets'] = res.iter_lines()
                else:
                    response['results'] = res.iter_lines()

                if 'x-matched-count' in res.headers:
                    response['size'] = res.headers['x-matched-count']
                elif action_request == 'search' and not res.ok:
                    response['size'] = 0

            return response

        except requests.exceptions.HTTPError as e:
            raise e

        except requests.exceptions.ConnectTimeout as e:
            raise e
        except requests.exceptions.ReadTimeout as e:
            raise e

        except requests.exceptions.Timeout as e:
            raise e

        except requests.exceptions.ConnectionError as e:
            raise e
