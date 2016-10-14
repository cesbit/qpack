import setuptools
from distutils.core import setup, Extension

module = Extension(
    'qpack._qpack',
    define_macros = [],
    include_dirs = ['./qpack'],
    libraries = [],
    sources = ['./qpack/_qpack.c']
)

VERSION = '0.0.9'

setup(
    name='qpack',
    packages=['qpack'],
    version=VERSION,
    description='QPack (de)serializer',
    author='Jeroen van der Heijden',
    author_email='jeroen@transceptor.technology',
    url='https://github.com/transceptor-technology/qpack',
    ext_modules = [module],
    download_url=
        'https://github.com/transceptor-technology/'
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
