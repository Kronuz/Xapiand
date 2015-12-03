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
from __future__ import absolute_import, unicode_literals

import json

from haystack import connections
from haystack.constants import ID, DJANGO_CT, DJANGO_ID
from haystack.backends import BaseEngine, BaseSearchBackend, BaseSearchQuery, log_query
from haystack.models import SearchResult
from haystack.utils import get_identifier, get_model_ct

from django.conf import settings
from django.utils.importlib import import_module
from django.core.exceptions import ImproperlyConfigured
from django.utils import six

from .client import Xapiand


DOCUMENT_TAGS_FIELD = 'tags'
DOCUMENT_AC_FIELD = 'ac'
KEY_FIELD_VALUE = '_value'
KEY_FIELD_WEIGHT = '_weight'
KEY_FIELD_INDEX = '_index'
KEY_FIELD_STORE = '_store'
TERM = 'term'
VALUE = 'value'
ALL = 'all'


class XapianSearchResults:
    def __init__(self, results):
        self.results = None
        self._first_result = None
        self.size = 0
        self.facets = dict()

        if 'results' in results:
            self.results = results['results']
            self.size = int(results['size'])

            for result in self.results:
                self._first_result = result
                break

        if 'facets' in results:
            self.facets = results['facets']

    def __len__(self):
        return self.size

    def __iter__(self):
        return self

    def next(self):
        if self._first_result is not None:
            _first_result, self._first_result = self._first_result, None
            return self.get_data(_first_result)
        else:
            next_result = self.results.next()
            if next_result is not None:
                return self.get_data(self.results.next())
            else:
                # for some reason the module requests keep-alive new lines as a None
                return self.resutls.next()
    __next__ = next

    def get_data(self, result):
        if result:
            result = json.loads(result)
            result = self.fix_data(result)
            result['model_name'] = result.pop('module_name')
            result['score'] = 0
            return SearchResult(**result)
        else:
            raise StopIteration

    def fix_data(self, result):
        _result = result
        for name in result:
            if '_value' in result[name]:
                _result[name] = result[name]['_value']
        return _result


class XapianSearchBackend(BaseSearchBackend):

    def __init__(self, connection_alias, language=None, **connection_options):
        super(XapianSearchBackend, self).__init__(connection_alias, **connection_options)

        endpoints = connection_options.get('ENDPOINTS')
        if isinstance(endpoints, six.string_types):
            router_module, _, router_class = endpoints.rpartition('.')
            router_module = import_module(router_module)
            router_class = getattr(router_module, router_class)
            endpoints = router_class()

        if not endpoints:
            raise ImproperlyConfigured("You must specify 'ENDPOINTS' in your settings for connection '%s'." % connection_alias)
        self.timeout = connection_options.get('TIMEOUT', None)
        self.servers = connection_options.get('SERVERS', '127.0.0.1:8880')
        self.language = language or connection_options.get('LANGUAGE', 'english')
        self.endpoints = endpoints

    def build_json(self, document_json, field_name, value, weight, index_type):
        # TODO: Any chance of multivalued?
        document_json[field_name] = {KEY_FIELD_VALUE: value, KEY_FIELD_INDEX: index_type, KEY_FIELD_WEIGHT: weight}

    def updater(self, index, obj, commit):
            if not obj.pk:
                return

            endpoints = self.endpoints.for_write(instance=obj)
            if not endpoints:
                return

            data = index.full_prepare(obj)
            weights = index.get_field_weights()

            document_json = {}
            for field in self.schema:
                field_name = field['field_name']

                if field_name in data:
                    values = data[field_name]

                    if not field['multi_valued']:
                        values = [values]

                    try:
                        weight = int(weights[field_name])
                    except KeyError:
                        weight = 1

                    if field_name in (ID, DJANGO_CT, DJANGO_ID):
                        field_name = field_name.upper()

                    field_type = field['type']
                    _field_name = field_name
                    for value in values:
                        if not value:
                            continue

                        if field_type == 'text':
                            if field['mode'] == 'autocomplete':  # mode = content, autocomplete, tagged
                                self.build_json(document_json, DOCUMENT_AC_FIELD, value, weight, TERM)
                                _field_name = DOCUMENT_AC_FIELD

                            elif field['mode'] == 'tagged':
                                self.build_json(document_json, DOCUMENT_TAGS_FIELD, value, weight, TERM)
                                _field_name = DOCUMENT_TAGS_FIELD

                            else:
                                self.build_json(document_json, field_name, value, weight, TERM)

                        elif field_type in ('ngram', 'edge_ngram'):
                                NGRAM_MIN_LENGTH = 1
                                NGRAM_MAX_LENGTH = 15
                                terms = _ngram_terms({value: weight}, min_length=NGRAM_MIN_LENGTH, max_length=NGRAM_MAX_LENGTH, split=field_type == 'edge_ngram')
                                for term, weight in terms.items():
                                    self.build_json(document_json, field_name, term, weight, TERM)

                        # FIXME: Xapiand support EWKT, by default use 'POINT' in the backend but needed change for receive a geometry primitives too
                        # FIXME: Xapiand no support list of POINTS, you need index MULTIPOINT to achieve it, for now it will overwrite the field POINT in case receive more that one
                        elif field_type == 'geo_point':
                            lat, _, lng = value.partition(',')
                            value = 'POINT(' + lat + ' ' + lng + ')'
                            self.build_json(document_json, field_name, value, weight, VALUE)

                        elif field_type == 'boolean':
                            self.build_json(document_json, field_name, value, weight, TERM)

                        elif field_type in ('integer', 'long'):
                            value = int(value)
                            self.build_json(document_json, field_name, value, weight, ALL)

                        elif field_type in ('float'):
                            value = float(value)
                            self.build_json(document_json, field_name, value, weight, ALL)

                        elif field_type in ('date'):
                            self.build_json(document_json, field_name, value, weight, ALL)

                        if field_name == self.content_field_name:
                            pass

                if 'stored' in field:
                        document_json.setdefault(_field_name, {KEY_FIELD_STORE: field['stored']})

            model = obj._meta.model
            if model._deferred:
                model = model._meta.proxy_for_model
            self.build_json(document_json, 'app_label', model._meta.app_label, weight, TERM)
            self.build_json(document_json, 'module_name', model._meta.module_name, weight, TERM)
            self.build_json(document_json, 'pk', obj.pk, weight, TERM)

            self.build_json(document_json, DJANGO_CT.upper(), get_model_ct(obj), 0, TERM)

            document_id = get_identifier(obj)
            client = Xapiand()
            ip = settings.XAPIAN_SERVER

            for endpoint in endpoints:
                client.send_request(action_request='index', endpoint=endpoint, params=dict(commit=True), ip=ip, data=document_json, document_id=document_id)

    def update(self, index, iterable, commit=False):
        for obj in iterable:
            self.updater(index, obj, commit=commit)

    def remove(self, obj, commit=False):
        endpoints = self.endpoints.for_write(instance=obj)
        document_id = get_identifier(obj)
        ip = settings.XAPIAN_SERVER
        client = Xapiand()
        for endpoint in endpoints:
            client.send_request(action_request='delete', endpoint=endpoint, ip=ip, document_id=document_id)

    def clear(self, models=[], commit=True):
        pass

    @log_query
    def search(self, query_string, start_offset, end_offset=None, queries=None, terms=None,
            partials=None, models=None, hints=None, **kwargs):
        """
        Returns:
            A dictionary with the following keys:
                `results` -- An iterator of `SearchResult`
                `hits` -- The total available results
                `facets` - A dictionary of facets
        """
        offset = start_offset
        limit = end_offset - start_offset
        if limit <= 0:
            return {
                'results': [],
                'hits': 0,
            }

        if models:
            if not terms:
                terms = set()
                for model in models:
                    terms.add('%s:%s.%s' % (DJANGO_CT.upper(), model._meta.app_label, model._meta.module_name))

        hints = hints or {}
        endpoints = self.endpoints.for_read(models=models, **hints)
        ip = settings.XAPIAN_SERVER
        params = dict(offset=offset, limit=limit, query=queries, terms=terms, partials=partials)

        client = Xapiand()
        results = client.send_request(action_request='search', endpoint=endpoints, params=params, ip=ip)
        results_obj = XapianSearchResults(results)

        return {
            'results': results_obj,
            'facet': results_obj.facets,
            'hits': results_obj.size,
        }

    def build_schema(self, fields):
        """
        Build the schema from fields.

        Required arguments:
            ``fields`` -- A list of fields in the index

        Returns a list of fields in dictionary format ready for inclusion in
        an indexed meta-data.
        """
        content_field_name = ''
        schema_fields = [
            {'field_name': ID, 'type': 'id', 'multi_valued': False, 'column': 0, 'stored': True, 'mode': None},
        ]
        column = len(schema_fields)

        for field_name, field_class in sorted(fields.items(), key=lambda n: n[0]):
            if field_class.document is True:
                content_field_name = field_class.index_fieldname

            if field_class.indexed is True:
                field_data = {
                    'field_name': field_class.index_fieldname,
                    'type': 'text',
                    'multi_valued': False,
                    'column': column,
                    'stored': field_class.stored,
                    'mode': field_class.mode,
                }

                if field_class.field_type in ['date', 'datetime']:
                    field_data['type'] = 'date'
                elif field_class.field_type == 'integer':
                    field_data['type'] = 'long'
                elif field_class.field_type == 'float':
                    field_data['type'] = 'float'
                elif field_class.field_type == 'boolean':
                    field_data['type'] = 'boolean'
                elif field_class.field_type == 'ngram':
                    field_data['type'] = 'ngram'
                elif field_class.field_type == 'edge_ngram':
                    field_data['type'] = 'edge_ngram'
                elif field_class.field_type == 'location':
                    field_data['type'] = 'geo_point'

                if field_class.is_multivalued:
                    field_data['multi_valued'] = True

                schema_fields.append(field_data)
                column += 1

        return (content_field_name, schema_fields)

    @property
    def schema(self):
        if not hasattr(self, '_schema'):
            fields = connections[self.connection_alias].get_unified_index().all_searchfields()
            self._content_field_name, self._schema = self.build_schema(fields)
        return self._schema

    @property
    def content_field_name(self):
        if not hasattr(self, '_content_field_name'):
            fields = connections[self.connection_alias].get_unified_index().all_searchfields()
            self._content_field_name, self._schema = self.build_schema(fields)
        return self._content_field_name


class XapianSearchQuery(BaseSearchQuery):
    def build_params(self, spelling_query=None):
        kwargs = super(XapianSearchQuery, self).build_params(spelling_query=spelling_query)
        if self.terms:
            kwargs['terms'] = self.terms
        if self.partials:
            kwargs['partials'] = self.partials
        if self.queries:
            kwargs['queries'] = self.queries
        return kwargs

    def build_query(self):
        self.partials = []
        self.queries = set()
        self.terms = set()
        return super(XapianSearchQuery, self).build_query()

    def build_query_fragment(self, field, filter_type, value):

        if filter_type == 'contains':
            self.queries.add('%s:%s' % (field, value))
            value = '###'

        # xapiand use operator AND_MAYBE with several partial, query and terms
        elif filter_type == 'like':
            for v in value.split():
                self.partials.append('%s:%s' % (field, v))
            value = '###'

        elif filter_type == 'exact':
            if field == DOCUMENT_AC_FIELD:
                for v in value.split():
                    self.partials.append('%s:%s' % (field, v))
                value = '###'
            elif field == DOCUMENT_TAGS_FIELD:
                value = '%s:%s' % (field, value)
                self.terms.add(value)
                value = '###'
            else:
                self.queries.add('%s:"%s"' % (field, value))
                value = '###'

        # FIXME: Xapiand support EWKT, It needs to implement a way to indicate which primitive is used
        # elif filter_type == 'geo':

        elif filter_type == 'gte':
            self.queries.add('%s:%s..' % (field, value))
            value = '###'

        elif filter_type == 'gt':
            self.queries.add('%s:..%s' % (field, value))
            self.queries.add('NOT %s' % '%s:%s' % (field, value))
            value = '###'

        elif filter_type == 'lte':
            self.queries.add('%s:..%s' % (field, value))
            value = '###'

        elif filter_type == 'lt':
            self.queries.add('%s:%s..' % (field, value))
            self.queries.add('NOT %s' % '%s:%s' % (field, value))
            value = '###'

        elif filter_type == 'startswith':
            self.queries.add('%s:%s*' % (field, value))
            value = '###'

        return value


class XapianEngine(BaseEngine):
    backend = XapianSearchBackend
    query = XapianSearchQuery


def _ngram_terms(terms, min_length=1, min_length_percentage=0, max_length=20, split=True):
    """
        :param terms: dictionary of (term, initial weight)
        :param min_length: Minimum ngram length
        :param max_length: Maximum ngram length
        :param min_length_percentage: Minimum length in percentage of the original term
    """
    split_terms = {}
    if not isinstance(terms, dict):
        terms = {terms: 1}

    for term, weight in terms.items():
        if term is not None:
            if split:
                _terms = term.split()
            else:
                _terms = [term]
            for _term in _terms:
                split_terms[_term] = max(split_terms.get(_term, 0), weight)

    # Find all the substrings of the term (all digit terms treated differently):
    final_terms = {}
    for term, weight in split_terms.items():
        term_len = len(term)
        for i in range(term_len):
            for j in range(i + 1, term_len + 1):
                l = j - i
                if l <= max_length and l >= min_length and l >= int(term_len * min_length_percentage):
                    _term = term[i:j]
                    _weight = int(float(weight * l) / term_len)
                    final_terms[_term] = max(final_terms.get(_term, 0), _weight)

    return final_terms
