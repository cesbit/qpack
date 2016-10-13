/*
 * _qpack.c
 *
 *  Created on: Oct 2, 2016
 *      Author: Jeroen van der Heijden
 */
#include <Python.h>
#include <inttypes.h>
#include <stddef.h>

#if PY_MAJOR_VERSION >= 3

#define PY_COMPAT_COMPARE(obj, str) PyUnicode_CompareWithASCIIString(obj, str)
#define PY_COMPAT_CHECK PyUnicode_Check
#define PY_DECODEUTF8(pt, size, error) PyUnicode_DecodeUTF8(pt, size, error)
#define PY_DECODELATIN1(pt, size, error) PyUnicode_DecodeLatin1(pt, size, error)
#define PYLONG_FROMLONGLONG(integer) PyLong_FromLongLong(integer)
#else

#define PY_COMPAT_COMPARE(obj, str) strcmp(PyString_AsString(obj), str) == 0
#define PY_COMPAT_CHECK PyString_Check
#define PY_DECODEUTF8(pt, size, error) PyString_Decode(pt, size, "utf-8", error)
#define PY_DECODELATIN1(pt, size, error) PyString_Decode(pt, size, "latin-1", error)
#define PYLONG_FROMLONGLONG(integer) PyInt_FromSize_t((size_t) integer)

#endif

static PyObject PY_ARRAY_CLOSE = {0};
static PyObject PY_MAP_CLOSE = {0};

#define Py_QPackCHECK(obj) (obj == &PY_ARRAY_CLOSE || obj == &PY_MAP_CLOSE)

typedef enum
{
    /*
     * Values with -##- will never be returned while unpacking. For example
     * a QP_INT8 (1 byte signed integer) will be returned as QP_INT64.
     */
    QP_END,             // at the end while unpacking
    QP_RAW,             // raw string
    /*
     * Both END and RAW are never actually packed but 0 and 1 are reserved
     * for positive signed integers.
     *
     * Fixed positive integers from 0 till 63       [  0...63  ]
     *
     * Fixed negative integers from -60 till -1     [ 64...123 ]
     *
     */
    QP_HOOK=124,        // Hook is not used by SiriDB
    QP_DOUBLE_N1=125,   // ## double value -1.0
    QP_DOUBLE_0,        // ## double value 0.0
    QP_DOUBLE_1,        // ## double value 1.0
    /*
     * Fixed raw strings lengths from 0 till 99     [ 128...227 ]
     */
    QP_RAW8=228,        // ## raw string with length < 1 byte
    QP_RAW16,           // ## raw string with length < 1 byte
    QP_RAW32,           // ## raw string with length < 1 byte
    QP_RAW64,           // ## raw string with length < 1 byte
    QP_INT8,            // ## 1 byte signed integer
    QP_INT16,           // ## 2 byte signed integer
    QP_INT32,           // ## 4 byte signed integer
    QP_INT64,           // 8 bytes signed integer
    QP_DOUBLE,          // 8 bytes double
    QP_ARRAY0,          // empty array
    QP_ARRAY1,          // array with 1 item
    QP_ARRAY2,          // array with 2 items
    QP_ARRAY3,          // array with 3 items
    QP_ARRAY4,          // array with 4 items
    QP_ARRAY5,          // array with 5 items
    QP_MAP0,            // empty map
    QP_MAP1,            // map with 1 item
    QP_MAP2,            // map with 2 items
    QP_MAP3,            // map with 3 items
    QP_MAP4,            // map with 4 items
    QP_MAP5,            // map with 5 items
    QP_TRUE,            // boolean true
    QP_FALSE,           // boolean false
    QP_NULL,            // null (none, nil)
    QP_ARRAY_OPEN,      // open a new array
    QP_MAP_OPEN,        // open a new map
    QP_ARRAY_CLOSE,     // close array
    QP_MAP_CLOSE,       // close map
} qp_types_t;

typedef enum
{
    DECODE_NONE,
    DECODE_UTF8,
    DECODE_LATIN1
} decode_t;

typedef struct
{
    char * buffer;
    Py_ssize_t size;
    Py_ssize_t len;
} packer_t;

#define DEFAULT_ALLOC_SZ 65536

#define PACKER_RESIZE(LEN)                                              \
if (packer->len + LEN > packer->size)                                   \
{                                                                       \
    packer->size = ((packer->len + LEN) / DEFAULT_ALLOC_SZ + 1)         \
            * DEFAULT_ALLOC_SZ;                                         \
    char * tmp = (char *) realloc(packer->buffer, packer->size);        \
    if (tmp == NULL)                                                    \
    {                                                                   \
        PyErr_SetString(PyExc_MemoryError, "Memory allocation error");  \
        packer->size = packer->len;                                     \
        return -1;                                                      \
    }                                                                   \
    packer->buffer = tmp;                                               \
}

#define UNPACK_CHECK_SZ(size)                                           \
if ((*pt) + size > end)                                                 \
{                                                                       \
    PyErr_SetString(PyExc_ValueError, "unpackb() is missing data");     \
    return NULL;                                                        \
}


#define UNPACK_RAW(size)                                \
UNPACK_CHECK_SZ(size)                                   \
switch(decode)                                          \
{                                                       \
case DECODE_NONE:                                       \
    obj = PyBytes_FromStringAndSize(*pt, size);         \
    break;                                              \
case DECODE_UTF8:                                       \
    obj = PY_DECODEUTF8(*pt, size, NULL);        \
    break;                                              \
case DECODE_LATIN1:                                     \
    obj = PY_DECODELATIN1(*pt, size, NULL);      \
    break;                                              \
}                                                       \
(*pt) += size;                                          \
return obj;

#define UNPACK_FIXED_RAW(uintx_t)                       \
{                                                       \
    UNPACK_CHECK_SZ(sizeof(uintx_t))                    \
    Py_ssize_t size = (Py_ssize_t) *((uintx_t *) *pt);  \
    (*pt) += sizeof(uintx_t);                           \
    UNPACK_RAW(size)                                    \
}

#define UNPACK_INT(intx_t)                              \
{                                                       \
    UNPACK_CHECK_SZ(sizeof(intx_t))                     \
    long long integer = (long long) *((intx_t *) *pt);  \
    (*pt) += sizeof(intx_t);                            \
    obj = PYLONG_FROMLONGLONG(integer);                 \
    return obj;                                         \
}

#define SET_UNEXPECTED(obj)                                                 \
if (Py_QPackCHECK(obj))                                                     \
{                                                                           \
    PyErr_SetString(                                                        \
            PyExc_ValueError,                                               \
            "unpackb() found an unexpected array or map close character");  \
}

/* Documentation strings */
static char module_docstring[] =
    "QPack - Python module in C";

static char packb_docstring[] =
    "Serialize a Python object to QPack format.";

static char unpackb_docstring[] =
"De-serialize QPack data to a Python object.\n"
"\n"
"Keyword arguments:\n"
"    decode: Decoding used for de-serializing QPack raw data.\n"
"            When None, all raw data will be de-serialized to Python bytes.\n"
"            (Default value: None)";

/* Available functions */
static PyObject * _qpack_packb(
        PyObject * self,
        PyObject * args,
        PyObject * kwargs);
static PyObject * _qpack_unpackb(
        PyObject * self,
        PyObject * args,
        PyObject * kwargs);

/* other static methods */
static packer_t * packer_new(void);
static void packer_free(packer_t * packer);
static int add_raw(packer_t * packer, const char * buffer, Py_ssize_t size);
static int packb(PyObject * obj, packer_t * packer);
static PyObject * unpackb(
        char ** pt,
        const char * const end,
        decode_t decode);

/* Module specification */
static PyMethodDef module_methods[] =
{
    {
            "_packb",
            (PyCFunction)_qpack_packb,
            METH_VARARGS | METH_KEYWORDS,
            packb_docstring
    },
    {
            "_unpackb",
            (PyCFunction)_qpack_unpackb,
            METH_VARARGS | METH_KEYWORDS,
            unpackb_docstring
    },
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "_qpack",          /* m_name */
        module_docstring,  /* m_doc */
        -1,                /* m_size */
        module_methods,    /* m_methods */
        NULL,              /* m_reload */
        NULL,              /* m_traverse */
        NULL,              /* m_clear */
        NULL,              /* m_free */
    };

    /* Initialize the module */
    PyMODINIT_FUNC PyInit__qpack(void)
    {
        return PyModule_Create(&moduledef);
    }
#else
    PyMODINIT_FUNC init_qpack(void)
    {
        PyObject *m = Py_InitModule3(
                "_qpack",
                module_methods,
                module_docstring);
        if (m == NULL) return;
    }
#endif


static packer_t * packer_new(void)
{
    packer_t * packer = (packer_t *) malloc(sizeof(packer_t));
    if (packer != NULL)
    {
        packer->size = DEFAULT_ALLOC_SZ;
        packer->len = 0;
        packer->buffer = (char *) malloc(DEFAULT_ALLOC_SZ);
        if (packer->buffer == NULL)
        {
            packer_free(packer);
            packer = NULL;
        }
    }
    return packer;
}

static void packer_free(packer_t * packer)
{
    free(packer->buffer);
    free(packer);
}

static int add_raw(packer_t * packer, const char * buffer, Py_ssize_t size)
{
    PACKER_RESIZE(5 + size)

    if (size < 100)
    {
        packer->buffer[packer->len++] = 128 + size;
    }
    else if (size < 256)
    {
        uint8_t length = (uint8_t) size;
        packer->buffer[packer->len++] = QP_RAW8;
        packer->buffer[packer->len++] = length;
    }
    else if (size < 65536)
    {
        uint16_t length = (uint16_t) size;
        packer->buffer[packer->len++] = QP_RAW16;
        memcpy(packer->buffer + packer->len, &length, sizeof(uint16_t));
        packer->len += sizeof(uint16_t);
    }
    else if (size < 4294967296)
    {
        uint32_t length = (uint32_t) size;
        packer->buffer[packer->len++] = QP_RAW32;
        memcpy(packer->buffer + packer->len, &length, sizeof(uint32_t));
        packer->len += sizeof(uint32_t);
    }
    else
    {
        uint64_t length = (uint64_t) size;
        packer->buffer[packer->len++] = QP_RAW64;
        memcpy(packer->buffer + packer->len, &length, sizeof(uint64_t));
        packer->len += sizeof(uint64_t);
    }

    memcpy(packer->buffer + packer->len, buffer, size);
    packer->len += size;

    return 0;
}

static int packb(PyObject * obj, packer_t * packer)
{
    if (obj == Py_True)
    {
        PACKER_RESIZE(1)
        packer->buffer[packer->len++] = QP_TRUE;
        return 0;
    }

    if (obj == Py_False)
    {
        PACKER_RESIZE(1)
        packer->buffer[packer->len++] = QP_FALSE;
        return 0;
    }

    if (obj == Py_None)
    {
        PACKER_RESIZE(1)
        packer->buffer[packer->len++] = QP_NULL;
        return 0;
    }

    if (PyList_Check(obj))
    {
        PACKER_RESIZE(1)

        Py_ssize_t size = PyList_GET_SIZE(obj);
        if (size < 6)
        {
            packer->buffer[packer->len++] = QP_ARRAY0 + size;

            for (Py_ssize_t i = 0; i < size; i++)
            {
                if (packb(PyList_GET_ITEM(obj, i), packer))
                {
                    return -1;  /* PyErr is set */
                }
            }
        }
        else
        {
            packer->buffer[packer->len++] = QP_ARRAY_OPEN;
            for (Py_ssize_t i = 0; i < size; i++)
            {
                if (packb(PyList_GET_ITEM(obj, i), packer))
                {
                    return -1;  /* PyErr is set */
                }
            }

            PACKER_RESIZE(1)
            packer->buffer[packer->len++] = QP_ARRAY_CLOSE;
        }
        return 0;
    }

    if (PyTuple_Check(obj))
    {
        PACKER_RESIZE(1)

        Py_ssize_t size = PyTuple_GET_SIZE(obj);
        if (size < 6)
        {
            packer->buffer[packer->len++] = QP_ARRAY0 + size;

            for (Py_ssize_t i = 0; i < size; i++)
            {
                if (packb(PyTuple_GET_ITEM(obj, i), packer))
                {
                    return -1;  /* PyErr is set */
                }
            }
        }
        else
        {
            packer->buffer[packer->len++] = QP_ARRAY_OPEN;
            for (Py_ssize_t i = 0; i < size; i++)
            {
                if (packb(PyTuple_GET_ITEM(obj, i), packer))
                {
                    return -1;  /* PyErr is set */
                }
            }

            PACKER_RESIZE(1)
            packer->buffer[packer->len++] = QP_ARRAY_CLOSE;
        }
        return 0;
    }

    if (PyDict_Check(obj))
    {
        PACKER_RESIZE(1)

        PyObject * key;
        PyObject * value;
        Py_ssize_t pos = 0;
        Py_ssize_t size = PyDict_Size(obj);

        if (size < 6)
        {
            packer->buffer[packer->len++] = QP_MAP0 + size;
            while (PyDict_Next(obj, &pos, &key, &value))
            {
                if (packb(key, packer) || packb(value, packer))
                {
                    return -1;  /* PyErr is set */
                }
            }
        }
        else
        {
            packer->buffer[packer->len++] = QP_MAP_OPEN;
            while (PyDict_Next(obj, &pos, &key, &value))
            {
                if (packb(key, packer) || packb(value, packer))
                {
                    return -1;  /* PyErr is set */
                }
            }
            PACKER_RESIZE(1)
            packer->buffer[packer->len++] = QP_MAP_CLOSE;
        }
        return 0;
    }

#if PY_MAJOR_VERSION >= 3
    if (PyLong_Check(obj))
    {
        /* An Overflow Error might be raised */
        int64_t i64 = PyLong_AsLongLong(obj);
#else
    if (PyLong_Check(obj) || PyInt_Check(obj))
    {
        /* An Overflow Error might be raised */
        int64_t i64 = PyLong_Check(obj) ?
                PyLong_AsLongLong(obj) : (long long) PyInt_AsLong(obj);

#endif
        int8_t i8;
        if ((i8 = i64) == i64)
        {
            PACKER_RESIZE(2)

            if (i8 >= 0 && i8 < 64)
            {
                packer->buffer[packer->len++] = i8;
            }
            else if (i8 >= -60 && i8 < 0)
            {
                packer->buffer[packer->len++] = 63 - i8;
            }
            else
            {
                packer->buffer[packer->len++] = QP_INT8;
                packer->buffer[packer->len++] = i8;
            }
            return 0;
        }
        int16_t i16;
        if ((i16 = i64) == i64)
        {
            PACKER_RESIZE(3)

            packer->buffer[packer->len++] = QP_INT16;
            memcpy(packer->buffer + packer->len, &i16, sizeof(int16_t));
            packer->len += sizeof(int16_t);
            return 0;
        }
        int32_t i32;
        if ((i32 = i64) == i64)
        {
            PACKER_RESIZE(5)

            packer->buffer[packer->len++] = QP_INT32;
            memcpy(packer->buffer + packer->len, &i32, sizeof(int32_t));
            packer->len += sizeof(int32_t);
            return 0;
        }

        PACKER_RESIZE(9)

        packer->buffer[packer->len++] = QP_INT64;
        memcpy(packer->buffer + packer->len, &i64, sizeof(int64_t));
        packer->len += sizeof(int64_t);

        return 0;
    }

    if (PyFloat_Check(obj))
    {
        double d = PyFloat_AsDouble(obj);
        if (d == -1.0)
        {
            PACKER_RESIZE(1)
            packer->buffer[packer->len++] = QP_DOUBLE_N1;
        }
        else if (d == 0.0)
        {
            PACKER_RESIZE(1)
            packer->buffer[packer->len++] = QP_DOUBLE_0;
        }
        else if (d == 1.0)
        {
            PACKER_RESIZE(1)
            packer->buffer[packer->len++] = QP_DOUBLE_1;
        }
        else
        {
            PACKER_RESIZE(9)
            packer->buffer[packer->len++] = QP_DOUBLE;
            memcpy(packer->buffer + packer->len, &d, sizeof(double));
            packer->len += sizeof(double);
        }

        return 0;
    }

    if (PY_COMPAT_CHECK(obj))
    {
        Py_ssize_t size;
#if PY_MAJOR_VERSION >= 3
        char * raw = PyUnicode_AsUTF8AndSize(obj, &size);
        return (raw == NULL) ? -1 : add_raw(packer, raw, size);
#else
        char * raw;
        return (PyString_AsStringAndSize(obj, &raw, &size) == -1) ?
                -1 : add_raw(packer, raw, size);
#endif
    }

    if (PyBytes_Check(obj))
    {
        Py_ssize_t size;
        char * buffer;
        return (PyBytes_AsStringAndSize(obj, &buffer, &size) == -1) ?
                -1 : add_raw(packer, buffer, size);
    }

    return 0;
}

static PyObject * unpackb(
        char ** pt,
        const char * const end,
        decode_t decode)
{
    PyObject * obj;

    uint8_t tp = **pt;
    (*pt)++;

    switch (tp)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
    case 62:
    case 63:
#if PY_MAJOR_VERSION >= 3
        obj = PyLong_FromLong((long) tp);
#else
        obj = PyInt_FromLong((long) tp);
#endif
        return obj;

    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
    case 79:
    case 80:
    case 81:
    case 82:
    case 83:
    case 84:
    case 85:
    case 86:
    case 87:
    case 88:
    case 89:
    case 90:
    case 91:
    case 92:
    case 93:
    case 94:
    case 95:
    case 96:
    case 97:
    case 98:
    case 99:
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 106:
    case 107:
    case 108:
    case 109:
    case 110:
    case 111:
    case 112:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 119:
    case 120:
    case 121:
    case 122:
    case 123:
#if PY_MAJOR_VERSION >= 3
        obj = PyLong_FromLong((long) 63 - tp);
#else
        obj = PyInt_FromLong((long) 63 - tp);
#endif
        return obj;

    case 124:
        /* This is actually reserved for an object hook but
         * return None as this is not implemented yet */
        Py_INCREF(Py_None);
        return Py_None;

    case 125:
        obj = PyFloat_FromDouble(-1.0);
        return obj;

    case 126:
        obj = PyFloat_FromDouble(0.0);
        return obj;

    case 127:
        obj = PyFloat_FromDouble(1.0);
        return obj;

    case 128:
    case 129:
    case 130:
    case 131:
    case 132:
    case 133:
    case 134:
    case 135:
    case 136:
    case 137:
    case 138:
    case 139:
    case 140:
    case 141:
    case 142:
    case 143:
    case 144:
    case 145:
    case 146:
    case 147:
    case 148:
    case 149:
    case 150:
    case 151:
    case 152:
    case 153:
    case 154:
    case 155:
    case 156:
    case 157:
    case 158:
    case 159:
    case 160:
    case 161:
    case 162:
    case 163:
    case 164:
    case 165:
    case 166:
    case 167:
    case 168:
    case 169:
    case 170:
    case 171:
    case 172:
    case 173:
    case 174:
    case 175:
    case 176:
    case 177:
    case 178:
    case 179:
    case 180:
    case 181:
    case 182:
    case 183:
    case 184:
    case 185:
    case 186:
    case 187:
    case 188:
    case 189:
    case 190:
    case 191:
    case 192:
    case 193:
    case 194:
    case 195:
    case 196:
    case 197:
    case 198:
    case 199:
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
    case 208:
    case 209:
    case 210:
    case 211:
    case 212:
    case 213:
    case 214:
    case 215:
    case 216:
    case 217:
    case 218:
    case 219:
    case 220:
    case 221:
    case 222:
    case 223:
    case 224:
    case 225:
    case 226:
    case 227:
        {
            Py_ssize_t size = tp - 128;
            UNPACK_RAW(size)
        }
    case 228:
        UNPACK_FIXED_RAW(uint8_t)
    case 229:
        UNPACK_FIXED_RAW(uint16_t)
    case 230:
        UNPACK_FIXED_RAW(uint32_t)
    case 231:
        UNPACK_FIXED_RAW(uint64_t)

    case 232:
        UNPACK_INT(int8_t)
    case 233:
        UNPACK_INT(int16_t)
    case 234:
        UNPACK_INT(int32_t)
    case 235:
        UNPACK_INT(int64_t)

    case 236:
        UNPACK_CHECK_SZ(sizeof(double))
        obj = PyFloat_FromDouble((double) *((double *) *pt));
        (*pt) += sizeof(double);
        return obj;

    case 237:
    case 238:
    case 239:
    case 240:
    case 241:
    case 242:
        {
            PyObject * o;
            Py_ssize_t size = tp - 237;
            obj = PyList_New(size);
            if (obj != NULL)
            {
                for (Py_ssize_t i = 0; i < size; i++)
                {
                    UNPACK_CHECK_SZ(0)

                    o = unpackb(pt, end, decode);

                    if (o == NULL || Py_QPackCHECK(o))
                    {
                        SET_UNEXPECTED(o)
                        Py_DECREF(obj);
                        return NULL;
                    }

                    PyList_SET_ITEM(obj, i, o);
                }
            }
            return obj;
        }
    case 243:
    case 244:
    case 245:
    case 246:
    case 247:
    case 248:
        {
            int rc;
            PyObject * key;
            PyObject * value;
            Py_ssize_t size = tp - 243;
            obj = PyDict_New();
            if (obj != NULL)
            {
                while (size--)
                {
                    UNPACK_CHECK_SZ(0)

                    key = unpackb(pt, end, decode);

                    if (key == NULL || Py_QPackCHECK(key))
                    {
                        SET_UNEXPECTED(key)
                        Py_DECREF(obj);
                        return NULL;
                    }

                    UNPACK_CHECK_SZ(0)

                    value = unpackb(pt, end, decode);

                    if (value == NULL || Py_QPackCHECK(value))
                    {
                        SET_UNEXPECTED(value)
                        Py_DECREF(key);
                        Py_DECREF(obj);
                        return NULL;
                    }

                    rc = PyDict_SetItem(obj, key, value);

                    Py_DECREF(key);
                    Py_DECREF(value);

                    if (rc == -1)
                    {
                        Py_DECREF(obj);
                        return NULL;
                    }
                }
            }
            return obj;
        }
    case 249:
        Py_INCREF(Py_True);
        return Py_True;

    case 250:
        Py_INCREF(Py_False);
        return Py_False;

    case 251:
        Py_INCREF(Py_None);
        return Py_None;

    case 252:
        {
            int rc;
            PyObject * o;
            obj = PyList_New(0);
            if (obj != NULL)
            {
                while (*pt < end)
                {
                    UNPACK_CHECK_SZ(0)

                    o = unpackb(pt, end, decode);

                    if (o == NULL || o == &PY_MAP_CLOSE)
                    {
                        SET_UNEXPECTED(o)
                        Py_DECREF(obj);
                        return NULL;
                    }
                    else if (o == &PY_ARRAY_CLOSE)
                    {
                        break;
                    }

                    rc = PyList_Append(obj, o);

                    Py_DECREF(o);

                    if (rc == -1)
                    {
                        Py_DECREF(obj);
                        return NULL;
                    }
                }
            }
            return obj;
        }
    case 253:
        {
            int rc;
            PyObject * key;
            PyObject * value;
            obj = PyDict_New();
            if (obj != NULL)
            {
                while (*pt < end)
                {
                    UNPACK_CHECK_SZ(0)

                    key = unpackb(pt, end, decode);

                    if (key == NULL || key == &PY_ARRAY_CLOSE)
                    {
                        SET_UNEXPECTED(key)
                        Py_DECREF(obj);
                        return NULL;
                    }
                    else if (value == &PY_MAP_CLOSE)
                    {
                        break;
                    }

                    UNPACK_CHECK_SZ(0)

                    value = unpackb(pt, end, decode);

                    if (value == NULL || Py_QPackCHECK(value))
                    {
                        SET_UNEXPECTED(value)
                        Py_DECREF(key);
                        Py_DECREF(obj);
                        return NULL;
                    }

                    rc = PyDict_SetItem(obj, key, value);

                    Py_DECREF(key);
                    Py_DECREF(value);

                    if (rc == -1)
                    {
                        Py_DECREF(obj);
                        return NULL;
                    }
                }
            }
            return obj;
        }
    case 254:
        return &PY_ARRAY_CLOSE;
    case 255:
        return &PY_MAP_CLOSE;

    }

    return NULL;
}

static PyObject * _qpack_packb(
        PyObject * self,
        PyObject * args,
        PyObject * kwargs)
{
    PyObject * packed;
    PyObject * obj;
    Py_ssize_t size;
    packer_t * packer;

    packer = packer_new();
    if (packer == NULL)
    {
        PyErr_SetString(PyExc_MemoryError, "Memory allocation error");
        return NULL;
    }

    size = PyTuple_GET_SIZE(args);

    if (size != 1)
    {
        PyErr_SetString(
                PyExc_TypeError,
                "packb() missing 1 required positional argument: 'o'");
        packer_free(packer);
        return NULL;
    }

    obj = PyTuple_GET_ITEM(args, 0);

    packed = (packb(obj, packer)) ?
            NULL: PyBytes_FromStringAndSize(packer->buffer, packer->len);

    packer_free(packer);
    return packed;
}

static PyObject * _qpack_unpackb(
        PyObject * self,
        PyObject * args,
        PyObject * kwargs)
{
    PyObject * obj;
    PyObject * kw_decode;
    PyObject * o_decode;
    PyObject * unpacked;
    Py_ssize_t size;
    decode_t decode = DECODE_NONE;

    size = PyTuple_GET_SIZE(args);

    if (size != 1)
    {
        PyErr_SetString(
                PyExc_TypeError,
                "unpackb(), exactly one positional argument is expected");
        return NULL;
    }

    obj = PyTuple_GET_ITEM(args, 0);

    if (kwargs)
    {
        kw_decode = Py_BuildValue("s", "decode");
        o_decode = PyDict_GetItem(kwargs, kw_decode);
        Py_DECREF(kw_decode);

        if (o_decode == NULL)
        {
            PyErr_SetString(
                    PyExc_TypeError,
                    "unpackb() got an unexpected keyword argument");
            return NULL;
        }

        if (PY_COMPAT_CHECK(o_decode))
        {
            if (    PY_COMPAT_COMPARE(o_decode, "utf-8") ||
                    PY_COMPAT_COMPARE(o_decode, "UTF-8") ||
                    PY_COMPAT_COMPARE(o_decode, "Utf-8") ||
                    PY_COMPAT_COMPARE(o_decode, "utf8") ||
                    PY_COMPAT_COMPARE(o_decode, "UTF8") ||
                    PY_COMPAT_COMPARE(o_decode, "Utf8"))
            {
                decode = DECODE_UTF8;
            }
            else if(PY_COMPAT_COMPARE(o_decode, "latin-1") ||
                    PY_COMPAT_COMPARE(o_decode, "LATIN-1") ||
                    PY_COMPAT_COMPARE(o_decode, "Latin-1") ||
                    PY_COMPAT_COMPARE(o_decode, "latin1") ||
                    PY_COMPAT_COMPARE(o_decode, "LATIN1") ||
                    PY_COMPAT_COMPARE(o_decode, "Latin1"))
            {
                decode = DECODE_LATIN1;
            }
            else
            {
                PyErr_SetString(
                        PyExc_LookupError,
                        "unpackb() unsupported encoding");
                return NULL;
            }
        }
        else if (o_decode != Py_None)
        {
            PyErr_SetString(
                    PyExc_LookupError,
                    "unpackb() decode is expecting 'None' or a 'str' object");
            return NULL;
        }
    }

    char * buffer;

    if (PyBytes_Check(obj))
    {
        if (PyBytes_AsStringAndSize(obj, &buffer, &size) == -1)
        {
            return NULL;  /* PyErr is set */
        }
    }
    else if (PyByteArray_Check(obj))
    {
        buffer = PyByteArray_AS_STRING(obj);
        size = PyByteArray_GET_SIZE(obj);
    }
    else
    {
        PyErr_SetString(
                PyExc_TypeError,
                "unpackb(), a bytes-like object is required");
        return NULL;
    }



    unpacked = unpackb(&buffer, buffer + size, decode);

    return unpacked;
}
