# -*- coding: utf-8 -*-
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
# Copyright © 2015-2020 Dubalu LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

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
