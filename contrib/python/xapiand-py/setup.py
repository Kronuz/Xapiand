# -*- coding: utf-8 -*-

import re
from os.path import join, dirname
from setuptools import setup, find_packages

with open(join(dirname(__file__), "README.md"), "r") as f:
    long_description = f.read().strip()

with open(join(dirname(__file__), "xapiand", "__init__.py"), "r") as f:
    __versionstr__ = '.'.join(re.search(r"^VERSION = \((\d+), (\d+), (\d+)\)", f.read(), re.M).groups())

install_requires = [
    'aiohttp>=3.6,<4',
    'msgpack_python>=0.5',
]

setup(
    name="xapiand",
    version=__versionstr__,
    author="Germán Méndez Bravo (Kronuz)",
    author_email="german.mb@gmail.com",
    url="https://github.com/Kronuz/Xapiand",
    license="Apache License, Version 2.0",
    description="Async Python client for Xapiand",
    long_description=long_description,
    long_description_content_type="text/markdown",
    python_requires=">=3.7",
    classifiers = [
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: Apache Software License",
        "Intended Audience :: Developers",
        "Operating System :: OS Independent",
        "Framework :: AsyncIO",
        "Programming Language :: Python",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: Python :: Implementation :: PyPy",
    ],
    install_requires=install_requires,
    packages=find_packages(where=".", exclude=("example",)),
)
