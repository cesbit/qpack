'''QPack - (de)serializer

Changelog

Version 0.0.2
    - Added support for hooks.
    - Fixed Python 2 Compatibility bug.

:copyright: 2016, Jeroen van der Heijden (Transceptor Technology)
'''

from .qpack import packb, unpackb

__version_info__ = (0, 0, 2)
__version__ = '.'.join(map(str, __version_info__))
__all__ = ['packb', 'unpackb']

