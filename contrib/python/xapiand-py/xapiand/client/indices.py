from .utils import NamespacedClient, query_params, _make_path, SKIP_IN_PATH


class IndicesClient(NamespacedClient):
    @query_params('timeout')
    def commit(self, index=None, params=None):
        """
        Explicitly commit one or more index, making all operations performed
        since the last commit available for search.

        :arg index: A comma-separated list of index names; use `_all` or empty
            string to perform the operation on all indices
        :arg timeout: Explicit operation timeout
        """
        return self.transport.perform_request('POST', _make_path(index,
            ':refresh'), params=params)

    @query_params('timeout')
    def open(self, index, params=None):
        """
        Open a closed index to make it available for search.

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('POST', _make_path(index,
            ':open'), params=params)

    @query_params('timeout')
    def close(self, index, params=None):
        """
        Close an index to remove it's overhead from the cluster. Closed index
        is blocked for read/write operations.

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('POST', _make_path(index,
            ':close'), params=params)

    @query_params('timeout')
    def delete(self, index, params=None):
        """
        Delete an index in Xapiand

        :arg index: The name of the index
        :arg timeout: Explicit operation timeout
        """
        if index in SKIP_IN_PATH:
            raise ValueError("Empty value passed for a required argument 'index'.")
        return self.transport.perform_request('POST', _make_path(index,
            ':delete'), params=params)
