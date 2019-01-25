# Python Xapiand Client

Official low-level client for Xapiand. Its goal is to provide common
ground for all Xapiand-related code in Python; because of this it tries
to be opinion-free and very extendable.


## Installation

Install the `xapiand` package with
[pip](https://pypi.python.org/pypi/xapiand):

    pip install xapiand


## Example use

Simple use-case:

```python
>>> from datetime import datetime
>>> from xapiand import Xapiand

# by default we connect to localhost:8880
>>> client = Xapiand()

# datetimes will be serialized
>>> client.index(index="my-index", id=42, body={"any": "data", "timestamp": datetime.now()})
{'timestamp': '2019-01-25T11:46:20.819478', '_id': 42, 'any': 'data', '#commit': False}

# and deserialized
>>> client.get(index="my-index", id=42)['_source']
{'timestamp': '2019-01-25T11:46:20.819478', '#docid': 1, '_id': 42, 'any': 'data'}
```

[Full documentation](https://kronuz.io/Xapiand/).


## Features

The client's features include:

* translating basic Python data types to and from msgpack
* configurable automatic discovery of cluster nodes
* persistent connections
* load balancing (with pluggable selection strategy) across all
  available nodes
* failed connection penalization (time based - failed connections
  won't be retried until a timeout is reached)
* thread safety
* pluggable architecture


## License

Copyright 2018-2019 Dubalu LLC Copyright 2017 Xapiand

Licensed under the Apache License, Version 2.0 (the "License"); you
may not use this file except in compliance with the License. You may
obtain a copy of the License at

[http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0)

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
