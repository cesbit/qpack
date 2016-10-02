import imp
import qpack as qp
import time
import msgpack
qpack = imp.load_dynamic('qpack', 'qpack-x86_64.cpython-35m-x86_64-linux-gnu.so')

COUNT = 10000

data = [1, 2, 3, {'b': True}, {'What?': 'Iriske is lief', 'Hoe oud is ze?': 3}]

data = {
    'series float': [
        [1471254705, 0.1],
        [1471254707, 0.0],
        [1471254710, 0.0]],
    'series integer': [
        [1471254705, 5],
        [1471254708, -3],
        [1471254710, -7]],
    'aggr': [
        [1447249033, 531], [1447249337, 0.0], [1447249633, 535], [1447249937, 0.0],
        [1447250249, 532], [1447250549, 0.0], [1447250868, 530], [1447251168, 0.0],
        [1447251449, 54], [1447251749, 0.0], [1447252049, 0.0], [1447252349, 0.0],
        [1447252649, 528], [1447252968, 0.0], [1447253244, 0.0], [1447253549, 0.0],
        [1447253849, 534], [1447254149, 0.0], [1447254449, 0.0], [1447254748, 0.0]
    ]
}

print(data)
import sys

a = msgpack.packb(data)
b = qp.packb(data)
c = qpack._packb(data)

print(a)
print(b)
print(c)

print(len(a), sys.getrefcount(a))
print(len(b), sys.getrefcount(b))
print(len(c), sys.getrefcount(c))

start = time.time()
for i in range(COUNT):
    msgpack.packb(data)
print('MsgPack (C-module) Time: {}'.format(time.time() - start))

start = time.time()
for i in range(COUNT):
    qp.packb(data)
print('QPack (Python)     Time: {}'.format(time.time() - start))

start = time.time()
for i in range(COUNT):
    qpack._packb(data)
print('QPack (C-module)   Time: {}'.format(time.time() - start))


a = msgpack.unpackb(a)
b = qp.unpackb(b)
c = qp.unpackb(c)

print(a)
print(b)
print(c)