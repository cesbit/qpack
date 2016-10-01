'''QPack - (de)serializer

:copyright: 2016, Jeroen van der Heijden (Transceptor Technology)
'''

__version_info__ = (0, 0, 1)
__version__ = '.'.join(map(str, __version_info__))

from .qpack import packb, unpackb