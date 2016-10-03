'''QPack - (de)serializer

Changelog

Version 0.0.4
    - Fixed bug in installing this package using pip.

Version 0.0.3
    - Added C module (Only Python3 support)

Version 0.0.2
    - Added support for hooks.
    - Fixed Python 2 Compatibility bug.

:copyright: 2016, Jeroen van der Heijden (Transceptor Technology)
'''
try:
    import qpack._qpack as _qpack
    packb = _qpack._packb
    unpackb = _qpack._unpackb

except ImportError as ex:
    from .fallback import packb, unpackb

__version_info__ = (0, 0, 4)
__version__ = '.'.join(map(str, __version_info__))
__all__ = ['packb', 'unpackb']

