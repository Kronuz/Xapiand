from ..exceptions import XapiandException


class BulkIndexError(XapiandException):
    @property
    def errors(self):
        """List of errors from execution of the last chunk."""
        return self.args[1]


class ScanError(XapiandException):
    def __init__(self, scroll_id, *args, **kwargs):
        super(ScanError, self).__init__(*args, **kwargs)
        self.scroll_id = scroll_id
