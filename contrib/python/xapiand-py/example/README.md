# Example code for Python Xapiand Client

This example code demonstrates the features and use patterns for the Python
client.

To run this example make sure you have Xapiand running on port 8880, install
additional dependencies (on top of xapiand-py):

```sh
pip install python-dateutil
``
or

```sh
pip install -r requirements.txt
```

And now you can load the index (the index will be called `git`):

```sh
python load.py
```

This will create an index with mappings and parse the git information of this
repository and load all the commits into it.

You can run some sample queries by running:

```sh
python queries.py
```

Look at the `queries.py` file for querying example and `load.py` on examples on
loading data into Xapiand.

Both `load` and `queries` set up logging so in `/tmp/xapiand_trace.log` you will
have a transcript of the commands being run in the curl format.
