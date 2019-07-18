# -*- coding: utf-8 -*-

from os.path import join, dirname
from setuptools import setup, find_packages

from xapiand import __versionstr__

with open(join(dirname(__file__), "README.md"), "r") as f:
    long_description = f.read().strip()

install_requires = [
    'urllib3>=1.21.1',
    'msgpack_python>=0.5',
]

setup(
    name="xapiand",
    version=__versionstr__,
    author="Germán Méndez Bravo (Kronuz)",
    author_email="german.mb@gmail.com",
    url="https://github.com/Kronuz/Xapiand",
    license="Apache License, Version 2.0",
    description="Python client for Xapiand",
    long_description=long_description,
    long_description_content_type="text/markdown",
    classifiers = [
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: Apache Software License",
        "Intended Audience :: Developers",
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Programming Language :: Python :: 2",
        "Programming Language :: Python :: 2.6",
        "Programming Language :: Python :: 2.7",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.2",
        "Programming Language :: Python :: 3.3",
        "Programming Language :: Python :: 3.4",
        "Programming Language :: Python :: 3.5",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: Implementation :: CPython",
        "Programming Language :: Python :: Implementation :: PyPy",
    ],
    install_requires=install_requires,
    packages=find_packages(where=".", exclude=("example",)),
    extras_require={
        'requests': ['requests>=2.4.0, <3.0.0']
    },
)
