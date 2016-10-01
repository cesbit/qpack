QPack
=====

QPack is a fast and efficient serialization format like MessagePack.
One key difference is the support for flexible maps and arrays which
allows code to write directly to a qpack buffer without to need in
front the size of a map or array.

>Warning: 
>--------
>This is still a BETA version which means the qpack format
>might change in the future.

Pack
====

`qpack.packb(object)`

Unpack
====

Unpack serialized data. When decode is left None, each string
will be returned as bytes. 

`qpack.unpackb(qp, decode=None)`

Example
=======

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

