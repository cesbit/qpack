"""setup.py

Upload to PyPI:

Build
  python setup.py sdist bdist_wheel

Upload to Test PyPI
  twine upload --repository-url https://test.pypi.org/legacy/ dist/*

Upload to PyPI
  twine upload dist/*
"""
import setuptools
from distutils.core import setup, Extension
from qpack import __version__

module = Extension(
    'qpack._qpack',
    define_macros=[],
    include_dirs=['./qpack'],
    libraries=[],
    sources=['./qpack/_qpack.c'],
    extra_compile_args=["--std=c99", "-pedantic"]
)

VERSION = __version__

setup(
    name='qpack',
    packages=['qpack'],
    version=VERSION,
    description='QPack (de)serializer',
    author='Jeroen van der Heijden',
    author_email='jeroen@transceptor.technology',
    url='https://github.com/transceptor-technology/qpack',
    ext_modules=[module],
    download_url='https://github.com/transceptor-technology/'
        'qpack/tarball/{}'.format(VERSION),
    keywords=['serializer', 'deserializer'],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Environment :: Other Environment',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 3',
        'Topic :: Software Development'
    ],
)
