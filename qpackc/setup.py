import platform
from distutils.core import setup, Extension

architecture = {'64bit': 'x86_64', '32bit': 'i386'}[platform.architecture()[0]]

c_ext = Extension('qpack-{}'.format(architecture), [
    'qpack.c',
])

setup(
    ext_modules=[c_ext],
)
