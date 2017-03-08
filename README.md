QPack
=====

QPack is a fast and efficient serialization format like MessagePack.
One key difference is flexible map and array support which allows
to write directly to a qpack buffer without the need to know
the size for the map or array beforehand.


Installation
------------

From PyPI (recommend)

```
pip install qpack
```

From source code

```
python setup.py install
```

Pack
----

`qpack.packb(object)`

Unpack
----

Unpack serialized data. When decode is left None, each string
will be returned as bytes.

`qpack.unpackb(qp, decode=None)`


Example
-------

```python
import qpack

# define some test data
data = {'name': 'Iris', 'age': 3}

# serialize into qpack format
qp = qpack.packb(data)

# unpack the serialized data
unpacked = qpack.unpackb(qp, decode='utf-8')

# left see what we've got...
print(unpacked)  # {'name': 'Iris', 'age': 3}
```

