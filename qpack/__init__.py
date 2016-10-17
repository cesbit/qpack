'''QPack - (de)serializer

:copyright: 2016, Jeroen van der Heijden (Transceptor Technology)
:license: MIT
'''
try:
    import qpack._qpack as _qpack
    packb = _qpack._packb
    unpackb = _qpack._unpackb

except ImportError as ex:
    print(ex)
    from .fallback import packb, unpackb

__version_info__ = (0, 0, 10)
__version__ = '.'.join(map(str, __version_info__))
__all__ = ['packb', 'unpackb']

