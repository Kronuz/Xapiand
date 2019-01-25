# -*- coding: utf-8 -*-

try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup


VERSION = (1, 0, 0)
__version__ = '.'.join(map(str, VERSION))


def read(fname):
    try:
        with open(os.path.join(os.path.dirname(__file__), fname), "r") as fp:
            return fp.read().strip()
    except IOError:
        return ''

install_requires = [
    'urllib3>=1.21.1',
    'msgpack_python>=0.5',
]

setup(
    name="xapiand",
    version=__version__,
    author="Germán Méndez Bravo (Kronuz), Honza Král, Nick Lang",
    author_email="german.mb@gmail.com, honza.kral@gmail.com, nick@nicklang.com",
    url="https://github.com/Kronuz/Xapiand",
    license="Apache License, Version 2.0",
    description="Python client for Xapiand",
    long_description=read("README.md"),
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
    packages=['xapiand'],
    extras_require={
        'requests': ['requests>=2.4.0, <3.0.0']
    },
)
