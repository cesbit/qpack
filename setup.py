#
#  Got documentation from: http://peterdowns.com/posts/first-time-with-pypi.html
#
#   0. Update __init__.py
#       changelog + version
#
#   1. Create tag:
#       git tag 0.0.4 -m "Adds a tag so we can put this new version on PyPI."
#
#   2. Push tag:
#       git push --tags origin master
#
#   3. Upload your package to PyPI Test:
#       python3 setup.py register -r pypitest
#       python3 setup.py sdist upload -r pypitest
#
#   4. Upload to PyPI Live
#       python3 setup.py register -r pypi
#       python3 setup.py sdist upload -r pypi
#

from distutils.core import setup, Extension

module = Extension('qpack._qpack',
                    define_macros = [],
                    include_dirs = ['./qpack'],
                    libraries = [],
                    sources = ['./qpack/_qpack.c'])

setup(
    name='qpack',
    packages=['qpack'],
    version='0.0.4',
    description='QPack (de)serializer',
    author='Jeroen van der Heijden',
    author_email='jeroen@transceptor.technology',
    url='https://github.com/transceptor-technology/qpack',
    ext_modules = [module],
    download_url='https://github.com/transceptor-technology/qpack/qpack/0.0.4',
    keywords=['serializer', 'deserializer'],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Environment :: Web Environment',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: MIT License',
        'Operating System :: OS Independent',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 3',
        'Topic :: Database',
    ],
)
