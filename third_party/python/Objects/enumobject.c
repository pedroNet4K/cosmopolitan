/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=4 sts=4 sw=4 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Python 3                                                                     │
│ https://docs.python.org/3/license.html                                       │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "libc/assert.h"
#include "third_party/python/Include/abstract.h"
#include "third_party/python/Include/enumobject.h"
#include "third_party/python/Include/longobject.h"
#include "third_party/python/Include/methodobject.h"
#include "third_party/python/Include/modsupport.h"
#include "third_party/python/Include/object.h"
#include "third_party/python/Include/objimpl.h"
#include "third_party/python/Include/pyerrors.h"
#include "third_party/python/Include/pymacro.h"

typedef struct {
    PyObject_HEAD
    Py_ssize_t en_index;           /* current index of enumeration */
    PyObject* en_sit;          /* secondary iterator of enumeration */
    PyObject* en_result;           /* result tuple  */
    PyObject* en_longindex;        /* index for sequences >= PY_SSIZE_T_MAX */
} enumobject;

static PyObject *
enum_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    enumobject *en;
    PyObject *seq = NULL;
    PyObject *start = NULL;
    static char *kwlist[] = {"iterable", "start", 0};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:enumerate", kwlist,
                                     &seq, &start))
        return NULL;

    en = (enumobject *)type->tp_alloc(type, 0);
    if (en == NULL)
        return NULL;
    if (start != NULL) {
        start = PyNumber_Index(start);
        if (start == NULL) {
            Py_DECREF(en);
            return NULL;
        }
        assert(PyLong_Check(start));
        en->en_index = PyLong_AsSsize_t(start);
        if (en->en_index == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            en->en_index = PY_SSIZE_T_MAX;
            en->en_longindex = start;
        } else {
            en->en_longindex = NULL;
            Py_DECREF(start);
        }
    } else {
        en->en_index = 0;
        en->en_longindex = NULL;
    }
    en->en_sit = PyObject_GetIter(seq);
    if (en->en_sit == NULL) {
        Py_DECREF(en);
        return NULL;
    }
    en->en_result = PyTuple_Pack(2, Py_None, Py_None);
    if (en->en_result == NULL) {
        Py_DECREF(en);
        return NULL;
    }
    return (PyObject *)en;
}

static void
enum_dealloc(enumobject *en)
{
    PyObject_GC_UnTrack(en);
    Py_XDECREF(en->en_sit);
    Py_XDECREF(en->en_result);
    Py_XDECREF(en->en_longindex);
    Py_TYPE(en)->tp_free(en);
}

static int
enum_traverse(enumobject *en, visitproc visit, void *arg)
{
    Py_VISIT(en->en_sit);
    Py_VISIT(en->en_result);
    Py_VISIT(en->en_longindex);
    return 0;
}

static PyObject *
enum_next_long(enumobject *en, PyObject* next_item)
{
    static PyObject *one = NULL;
    PyObject *result = en->en_result;
    PyObject *next_index;
    PyObject *stepped_up;

    if (en->en_longindex == NULL) {
        en->en_longindex = PyLong_FromSsize_t(PY_SSIZE_T_MAX);
        if (en->en_longindex == NULL) {
            Py_DECREF(next_item);
            return NULL;
        }
    }
    if (one == NULL) {
        one = PyLong_FromLong(1);
        if (one == NULL) {
            Py_DECREF(next_item);
            return NULL;
        }
    }
    next_index = en->en_longindex;
    assert(next_index != NULL);
    stepped_up = PyNumber_Add(next_index, one);
    if (stepped_up == NULL) {
        Py_DECREF(next_item);
        return NULL;
    }
    en->en_longindex = stepped_up;

    if (result->ob_refcnt == 1) {
        Py_INCREF(result);
        Py_DECREF(PyTuple_GET_ITEM(result, 0));
        Py_DECREF(PyTuple_GET_ITEM(result, 1));
    } else {
        result = PyTuple_New(2);
        if (result == NULL) {
            Py_DECREF(next_index);
            Py_DECREF(next_item);
            return NULL;
        }
    }
    PyTuple_SET_ITEM(result, 0, next_index);
    PyTuple_SET_ITEM(result, 1, next_item);
    return result;
}

static PyObject *
enum_next(enumobject *en)
{
    PyObject *next_index;
    PyObject *next_item;
    PyObject *result = en->en_result;
    PyObject *it = en->en_sit;

    next_item = (*Py_TYPE(it)->tp_iternext)(it);
    if (next_item == NULL)
        return NULL;

    if (en->en_index == PY_SSIZE_T_MAX)
        return enum_next_long(en, next_item);

    next_index = PyLong_FromSsize_t(en->en_index);
    if (next_index == NULL) {
        Py_DECREF(next_item);
        return NULL;
    }
    en->en_index++;

    if (result->ob_refcnt == 1) {
        Py_INCREF(result);
        Py_DECREF(PyTuple_GET_ITEM(result, 0));
        Py_DECREF(PyTuple_GET_ITEM(result, 1));
    } else {
        result = PyTuple_New(2);
        if (result == NULL) {
            Py_DECREF(next_index);
            Py_DECREF(next_item);
            return NULL;
        }
    }
    PyTuple_SET_ITEM(result, 0, next_index);
    PyTuple_SET_ITEM(result, 1, next_item);
    return result;
}

static PyObject *
enum_reduce(enumobject *en)
{
    if (en->en_longindex != NULL)
        return Py_BuildValue("O(OO)", Py_TYPE(en), en->en_sit, en->en_longindex);
    else
        return Py_BuildValue("O(On)", Py_TYPE(en), en->en_sit, en->en_index);
}

PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");

static PyMethodDef enum_methods[] = {
    {"__reduce__", (PyCFunction)enum_reduce, METH_NOARGS, reduce_doc},
    {NULL,              NULL}           /* sentinel */
};

PyDoc_STRVAR(enum_doc,
"enumerate(iterable[, start]) -> iterator for index, value of iterable\n"
"\n"
"Return an enumerate object.  iterable must be another object that supports\n"
"iteration.  The enumerate object yields pairs containing a count (from\n"
"start, which defaults to zero) and a value yielded by the iterable argument.\n"
"enumerate is useful for obtaining an indexed list:\n"
"    (0, seq[0]), (1, seq[1]), (2, seq[2]), ...");

PyTypeObject PyEnum_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "enumerate",                    /* tp_name */
    sizeof(enumobject),             /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */
    (destructor)enum_dealloc,       /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_reserved */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                              /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    PyObject_GenericGetAttr,        /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,    /* tp_flags */
    enum_doc,                       /* tp_doc */
    (traverseproc)enum_traverse,    /* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)enum_next,        /* tp_iternext */
    enum_methods,                   /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    PyType_GenericAlloc,            /* tp_alloc */
    enum_new,                       /* tp_new */
    PyObject_GC_Del,                /* tp_free */
};

/* Reversed Object ***************************************************************/

typedef struct {
    PyObject_HEAD
    Py_ssize_t      index;
    PyObject* seq;
} reversedobject;

static PyObject *
reversed_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    Py_ssize_t n;
    PyObject *seq, *reversed_meth;
    reversedobject *ro;
    _Py_IDENTIFIER(__reversed__);

    if (type == &PyReversed_Type && !_PyArg_NoKeywords("reversed()", kwds))
        return NULL;

    if (!PyArg_UnpackTuple(args, "reversed", 1, 1, &seq) )
        return NULL;

    reversed_meth = _PyObject_LookupSpecial(seq, &PyId___reversed__);
    if (reversed_meth == Py_None) {
        Py_DECREF(reversed_meth);
        PyErr_Format(PyExc_TypeError,
                     "'%.200s' object is not reversible",
                     Py_TYPE(seq)->tp_name);
        return NULL;
    }
    if (reversed_meth != NULL) {
        PyObject *res = PyObject_CallFunctionObjArgs(reversed_meth, NULL);
        Py_DECREF(reversed_meth);
        return res;
    }
    else if (PyErr_Occurred())
        return NULL;

    if (!PySequence_Check(seq)) {
        PyErr_Format(PyExc_TypeError,
                     "'%.200s' object is not reversible",
                     Py_TYPE(seq)->tp_name);
        return NULL;
    }

    n = PySequence_Size(seq);
    if (n == -1)
        return NULL;

    ro = (reversedobject *)type->tp_alloc(type, 0);
    if (ro == NULL)
        return NULL;

    ro->index = n-1;
    Py_INCREF(seq);
    ro->seq = seq;
    return (PyObject *)ro;
}

static void
reversed_dealloc(reversedobject *ro)
{
    PyObject_GC_UnTrack(ro);
    Py_XDECREF(ro->seq);
    Py_TYPE(ro)->tp_free(ro);
}

static int
reversed_traverse(reversedobject *ro, visitproc visit, void *arg)
{
    Py_VISIT(ro->seq);
    return 0;
}

static PyObject *
reversed_next(reversedobject *ro)
{
    PyObject *item;
    Py_ssize_t index = ro->index;

    if (index >= 0) {
        item = PySequence_GetItem(ro->seq, index);
        if (item != NULL) {
            ro->index--;
            return item;
        }
        if (PyErr_ExceptionMatches(PyExc_IndexError) ||
            PyErr_ExceptionMatches(PyExc_StopIteration))
            PyErr_Clear();
    }
    ro->index = -1;
    Py_CLEAR(ro->seq);
    return NULL;
}

PyDoc_STRVAR(reversed_doc,
"reversed(sequence) -> reverse iterator over values of the sequence\n"
"\n"
"Return a reverse iterator");

static PyObject *
reversed_len(reversedobject *ro)
{
    Py_ssize_t position, seqsize;

    if (ro->seq == NULL)
        return PyLong_FromLong(0);
    seqsize = PySequence_Size(ro->seq);
    if (seqsize == -1)
        return NULL;
    position = ro->index + 1;
    return PyLong_FromSsize_t((seqsize < position)  ?  0  :  position);
}

PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");

static PyObject *
reversed_reduce(reversedobject *ro)
{
    if (ro->seq)
        return Py_BuildValue("O(O)n", Py_TYPE(ro), ro->seq, ro->index);
    else
        return Py_BuildValue("O(())", Py_TYPE(ro));
}

static PyObject *
reversed_setstate(reversedobject *ro, PyObject *state)
{
    Py_ssize_t index = PyLong_AsSsize_t(state);
    if (index == -1 && PyErr_Occurred())
        return NULL;
    if (ro->seq != 0) {
        Py_ssize_t n = PySequence_Size(ro->seq);
        if (n < 0)
            return NULL;
        if (index < -1)
            index = -1;
        else if (index > n-1)
            index = n-1;
        ro->index = index;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(setstate_doc, "Set state information for unpickling.");

static PyMethodDef reversediter_methods[] = {
    {"__length_hint__", (PyCFunction)reversed_len, METH_NOARGS, length_hint_doc},
    {"__reduce__", (PyCFunction)reversed_reduce, METH_NOARGS, reduce_doc},
    {"__setstate__", (PyCFunction)reversed_setstate, METH_O, setstate_doc},
    {NULL,              NULL}           /* sentinel */
};

PyTypeObject PyReversed_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "reversed",                     /* tp_name */
    sizeof(reversedobject),         /* tp_basicsize */
    0,                              /* tp_itemsize */
    /* methods */
    (destructor)reversed_dealloc,   /* tp_dealloc */
    0,                              /* tp_print */
    0,                              /* tp_getattr */
    0,                              /* tp_setattr */
    0,                              /* tp_reserved */
    0,                              /* tp_repr */
    0,                              /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                              /* tp_as_mapping */
    0,                              /* tp_hash */
    0,                              /* tp_call */
    0,                              /* tp_str */
    PyObject_GenericGetAttr,        /* tp_getattro */
    0,                              /* tp_setattro */
    0,                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,    /* tp_flags */
    reversed_doc,                   /* tp_doc */
    (traverseproc)reversed_traverse,/* tp_traverse */
    0,                              /* tp_clear */
    0,                              /* tp_richcompare */
    0,                              /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)reversed_next,    /* tp_iternext */
    reversediter_methods,               /* tp_methods */
    0,                              /* tp_members */
    0,                              /* tp_getset */
    0,                              /* tp_base */
    0,                              /* tp_dict */
    0,                              /* tp_descr_get */
    0,                              /* tp_descr_set */
    0,                              /* tp_dictoffset */
    0,                              /* tp_init */
    PyType_GenericAlloc,            /* tp_alloc */
    reversed_new,                   /* tp_new */
    PyObject_GC_Del,                /* tp_free */
};
