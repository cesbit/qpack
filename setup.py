#
#  Got documentation from: http://peterdowns.com/posts/first-time-with-pypi.html
#
#   0. Update __init__.py
#       changelog + version
#
#   1. Create tag:
#       git tag 0.0.2 -m "Adds a tag so that we can put this new version on PyPI."
#
#   2. Push tag:
#       git push --tags origin master
#
#   3. Upload your package to PyPI Test:
#       python setup.py register -r pypitest
#       python setup.py sdist upload -r pypitest
#
#   4. Upload to PyPI Live
#       python setup.py register -r pypi
#       python setup.py sdist upload -r pypi
#

from distutils.core import setup
setup(
    name='qpack',
    packages=['qpack'],
    version='0.0.2',
    description='QPack (de)serializer',
    author='Jeroen van der Heijden',
    author_email='jeroen@transceptor.technology',
    url='https://github.com/transceptor-technology/qpack',
    download_url='https://github.com/transceptor-technology/qpack/qpack/0.0.2',
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
