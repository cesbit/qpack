import imp
import qpack as qp
qpack = imp.load_dynamic('qpack', 'qpack-x86_64.cpython-35m-x86_64-linux-gnu.so')

d = [1, 2, 3, {'b': True}]

print(d)
a = qpack._packb(d)
b = qp.packb(d)

print(a)
print(b)


u1 = qp.unpackb(a)
u2 = qp.unpackb(b)

print(u1)
print(u2)