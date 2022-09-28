# -*- coding: utf-8 -*-
import sys
import qpack
from qpack import fallback
import unittest
import pickle

if sys.version_info[0] == 3:
    INT_CONVERT = int
    PYTHON3 = True
else:
    INT_CONVERT = ord
    PYTHON3 = False


class TestQpack(unittest.TestCase):

    CASES = [
        [u" Hi Qpack", [
            140, 239, 163, 159, 32, 72, 105, 32, 81, 112, 97, 99, 107]],
        [True, [249]],
        [False, [250]],
        [None, [251]],
        [-1, [64]],
        [-60, [123]],
        [-61, [232, 195]],
        [0, [0]],
        [1, [1]],
        [4, [4]],
        [63, [63]],
        [64, [232, 64]],
        [-1.0, [125]],
        [0.0, [126]],
        [1.0, [127]],
        [-120, [232, 136]],
        [-0xfe, [233, 2, 255]],
        [-0xfedcba, [234, 70, 35, 1, 255]],
        [-0xfedcba9876, [235, 138, 103, 69, 35, 1, 255, 255, 255]],
        [120, [232, 120]],
        [0xfe, [233, 254, 0]],
        [0xfedcba, [234, 186, 220, 254, 0]],
        [0xfedcba9876, [235, 118, 152, 186, 220, 254, 0, 0, 0]],
        [-1.234567, [236, 135, 136, 155, 83, 201, 192, 243, 191]],
        [123.4567, [236, 83, 5, 163, 146, 58, 221, 94, 64]],
        [[0.0, 1.1, 2.2], [
            240, 126, 236, 154, 153, 153, 153, 153, 153, 241, 63,
            236, 154, 153, 153, 153, 153, 153, 1, 64]],
        [[10, 20, 30, 40, 50], [242, 10, 20, 30, 40, 50]],
        [[10, 20, 30, 40, 50, 60], [
            252, 10, 20, 30, 40, 50, 60, 254]],
        [[0, {"Names": ["Iris", "Sasha"]}], [
            239, 0, 244, 133, 78, 97, 109, 101, 115, 239, 132, 73, 114,
            105, 115, 133, 83, 97, 115, 104, 97]]]

    def _pack(self, packb):
        for inp, want in self.CASES:
            out = packb(inp)
            self.assertEqual([INT_CONVERT(i) for i in out], want)

    def _unpack(self, unpackb):
        for inp, want in self.CASES:
            out = unpackb(qpack.packb(inp), decode='utf8')
            self.assertEqual(out, inp)

    def test_packb(self):
        self.assertEqual(
            qpack.packb.__doc__,
            'Serialize a Python object to QPack format.')
        self._pack(qpack.packb)

    def test_fallback_packb(self):
        self.assertEqual(
            fallback.packb.__doc__,
            'Serialize to QPack. (Pure Python implementation)')
        self._pack(fallback.packb)

    def test_unpackb(self):
        self.assertNotIn(
            'Pure Python implementation',
            qpack.unpackb.__doc__)
        self._unpack(qpack.unpackb)

    def test_fallback_unpackb(self):
        self.assertIn(
            'Pure Python implementation',
            fallback.unpackb.__doc__)
        self._unpack(fallback.unpackb)

    def test_packb_unsupported(self):
        with self.assertRaises(TypeError):
            fallback.packb({'module': sys})
        with self.assertRaises(TypeError):
            qpack.packb({'module': sys})

    def test_decode(self):
        if not PYTHON3:
            return
        bindata = pickle.dumps({})
        data = ['normal', bindata]
        packed = qpack.packb(data)
        s, b = qpack.unpackb(packed)
        self.assertEqual(type(s).__name__, 'bytes')
        self.assertEqual(type(b).__name__, 'bytes')

        with self.assertRaises(ValueError):
            s, b = qpack.unpackb(packed, decode='utf8')

        s, b = qpack.unpackb(packed, decode='utf8', ignore_decode_errors=True)
        self.assertEqual(type(s).__name__, 'str')
        self.assertEqual(type(b).__name__, 'bytes')

    def test_fallback_decode(self):
        if not PYTHON3:
            return
        bindata = pickle.dumps({})
        data = ['normal', bindata]
        packed = fallback.packb(data)
        s, b = fallback.unpackb(packed)
        self.assertEqual(type(s).__name__, 'bytes')
        self.assertEqual(type(b).__name__, 'bytes')

        with self.assertRaises(ValueError):
            s, b = fallback.unpackb(packed, decode='utf8')

        s, b = fallback.unpackb(
            packed, decode='utf8', ignore_decode_errors=True)
        self.assertEqual(type(s).__name__, 'str')
        self.assertEqual(type(b).__name__, 'bytes')

    def test_use_tuples(self):
        data = tuple([('a', 'b') for _ in range(20)])
        packed = qpack.packb(data)

        result = fallback.unpackb(packed, use_tuples=True)
        self.assertEqual(result, data)


if __name__ == '__main__':
    unittest.main()
