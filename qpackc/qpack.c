/*
 * qpack.c
 *
 *  Created on: Oct 2, 2016
 *      Author: joente
 */
#include <Python.h>
#include <inttypes.h>

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



/* Documentation strings */
static char module_docstring[] =
    "QPack - Python module in C";

static char packb_docstring[] =
    "Serialize a Python object to qpack format.";

static char unpackb_docstring[] =
    "De-Serialize qpack data to a Python object.\n"
    "\n"
    "Keyword arguments:\n"
    "    decode: Decode byte strings, for example to 'utf-8'.\n"
    "            \n"
    "            Default value: None";

/* Available functions */
static PyObject * qpack_packb(
        PyObject * self,
        PyObject * args,
        PyObject * kwargs);
static PyObject * qpack_unpackb(
        PyObject * self,
        PyObject * args,
        PyObject * kwargs);

/* other static methods */
static packer_t * packer_new(void);
static void packer_free(packer_t * packer);
static int add_raw(packer_t * packer, const char * buffer, Py_ssize_t size);
static int packb(PyObject * obj, packer_t * packer);

/* Module specification */
static PyMethodDef module_methods[] =
{
    {
            "_packb",
            (PyCFunction)qpack_packb,
            METH_VARARGS | METH_KEYWORDS,
            packb_docstring
    },
    {
            "_unpackb",
            (PyCFunction)qpack_unpackb,
            METH_VARARGS | METH_KEYWORDS,
            unpackb_docstring
    },
    {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "qpack",           /* m_name */
        module_docstring,  /* m_doc */
        -1,                /* m_size */
        module_methods,    /* m_methods */
        NULL,              /* m_reload */
        NULL,              /* m_traverse */
        NULL,              /* m_clear */
        NULL,              /* m_free */
    };

    /* Initialize the module */
    PyMODINIT_FUNC PyInit_qpack(void)
    {
        return PyModule_Create(&moduledef);
    }
#else
    PyMODINIT_FUNC initqpack(void)
    {
        PyObject *m = Py_InitModule3(
                "qpack",
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

    if (PyLong_Check(obj))
    {
        /* An Overflow Error might be raised */
        int64_t i64 = PyLong_AsLongLong(obj);
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

    if (PyUnicode_Check(obj))
    {
        Py_ssize_t size;
        char * raw = PyUnicode_AsUTF8AndSize(obj, &size);

        return (raw == NULL) ? -1 : add_raw(packer, raw, size);
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

static PyObject * qpack_packb(
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

static PyObject * qpack_unpackb(
        PyObject * self,
        PyObject * args,
        PyObject * kwargs)
{
    PyObject * unpacked;

    unpacked = NULL;

    return unpacked;
}
