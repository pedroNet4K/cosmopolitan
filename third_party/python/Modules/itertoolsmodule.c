/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:4;tab-width:8;coding:utf-8 -*-│
│ vi: set noet ft=c ts=4 sts=4 sw=4 fenc=utf-8                             :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Python 3                                                                     │
│ https://docs.python.org/3/license.html                                       │
╚─────────────────────────────────────────────────────────────────────────────*/
#define PY_SSIZE_T_CLEAN
#include "third_party/python/Include/abstract.h"
#include "third_party/python/Include/boolobject.h"
#include "third_party/python/Include/dictobject.h"
#include "third_party/python/Include/import.h"
#include "third_party/python/Include/longobject.h"
#include "third_party/python/Include/modsupport.h"
#include "third_party/python/Include/objimpl.h"
#include "third_party/python/Include/structmember.h"
#include "third_party/python/Include/tupleobject.h"
#include "third_party/python/Include/yoink.h"

PYTHON_PROVIDE("itertools");
PYTHON_PROVIDE("itertools._grouper");
PYTHON_PROVIDE("itertools._tee");
PYTHON_PROVIDE("itertools._tee_dataobject");
PYTHON_PROVIDE("itertools.accumulate");
PYTHON_PROVIDE("itertools.chain");
PYTHON_PROVIDE("itertools.combinations");
PYTHON_PROVIDE("itertools.combinations_with_replacement");
PYTHON_PROVIDE("itertools.compress");
PYTHON_PROVIDE("itertools.count");
PYTHON_PROVIDE("itertools.cycle");
PYTHON_PROVIDE("itertools.dropwhile");
PYTHON_PROVIDE("itertools.filterfalse");
PYTHON_PROVIDE("itertools.groupby");
PYTHON_PROVIDE("itertools.islice");
PYTHON_PROVIDE("itertools.permutations");
PYTHON_PROVIDE("itertools.product");
PYTHON_PROVIDE("itertools.repeat");
PYTHON_PROVIDE("itertools.starmap");
PYTHON_PROVIDE("itertools.takewhile");
PYTHON_PROVIDE("itertools.tee");
PYTHON_PROVIDE("itertools.zip_longest");

/* Itertools module written and maintained
   by Raymond D. Hettinger <python@rcn.com>
*/

/* groupby object ************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *it;
    PyObject *keyfunc;
    PyObject *tgtkey;
    PyObject *currkey;
    PyObject *currvalue;
} groupbyobject;

static PyTypeObject groupby_type;
static PyObject *_grouper_create(groupbyobject *, PyObject *);

static PyObject *
groupby_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *kwargs[] = {"iterable", "key", NULL};
    groupbyobject *gbo;
    PyObject *it, *keyfunc = Py_None;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:groupby", kwargs,
                                     &it, &keyfunc))
        return NULL;

    gbo = (groupbyobject *)type->tp_alloc(type, 0);
    if (gbo == NULL)
        return NULL;
    gbo->tgtkey = NULL;
    gbo->currkey = NULL;
    gbo->currvalue = NULL;
    gbo->keyfunc = keyfunc;
    Py_INCREF(keyfunc);
    gbo->it = PyObject_GetIter(it);
    if (gbo->it == NULL) {
        Py_DECREF(gbo);
        return NULL;
    }
    return (PyObject *)gbo;
}

static void
groupby_dealloc(groupbyobject *gbo)
{
    PyObject_GC_UnTrack(gbo);
    Py_XDECREF(gbo->it);
    Py_XDECREF(gbo->keyfunc);
    Py_XDECREF(gbo->tgtkey);
    Py_XDECREF(gbo->currkey);
    Py_XDECREF(gbo->currvalue);
    Py_TYPE(gbo)->tp_free(gbo);
}

static int
groupby_traverse(groupbyobject *gbo, visitproc visit, void *arg)
{
    Py_VISIT(gbo->it);
    Py_VISIT(gbo->keyfunc);
    Py_VISIT(gbo->tgtkey);
    Py_VISIT(gbo->currkey);
    Py_VISIT(gbo->currvalue);
    return 0;
}

Py_LOCAL_INLINE(int)
groupby_step(groupbyobject *gbo)
{
    PyObject *newvalue, *newkey, *oldvalue;

    newvalue = PyIter_Next(gbo->it);
    if (newvalue == NULL)
        return -1;

    if (gbo->keyfunc == Py_None) {
        newkey = newvalue;
        Py_INCREF(newvalue);
    } else {
        newkey = PyObject_CallFunctionObjArgs(gbo->keyfunc, newvalue, NULL);
        if (newkey == NULL) {
            Py_DECREF(newvalue);
            return -1;
        }
    }

    oldvalue = gbo->currvalue;
    gbo->currvalue = newvalue;
    Py_XSETREF(gbo->currkey, newkey);
    Py_XDECREF(oldvalue);
    return 0;
}

static PyObject *
groupby_next(groupbyobject *gbo)
{
    PyObject *r, *grouper;

    /* skip to next iteration group */
    for (;;) {
        if (gbo->currkey == NULL)
            /* pass */;
        else if (gbo->tgtkey == NULL)
            break;
        else {
            int rcmp;

            rcmp = PyObject_RichCompareBool(gbo->tgtkey, gbo->currkey, Py_EQ);
            if (rcmp == -1)
                return NULL;
            else if (rcmp == 0)
                break;
        }

        if (groupby_step(gbo) < 0)
            return NULL;
    }
    Py_INCREF(gbo->currkey);
    Py_XSETREF(gbo->tgtkey, gbo->currkey);

    grouper = _grouper_create(gbo, gbo->tgtkey);
    if (grouper == NULL)
        return NULL;

    r = PyTuple_Pack(2, gbo->currkey, grouper);
    Py_DECREF(grouper);
    return r;
}

static PyObject *
groupby_reduce(groupbyobject *lz)
{
    /* reduce as a 'new' call with an optional 'setstate' if groupby
     * has started
     */
    PyObject *value;
    if (lz->tgtkey && lz->currkey && lz->currvalue)
        value = Py_BuildValue("O(OO)(OOO)", Py_TYPE(lz),
            lz->it, lz->keyfunc, lz->currkey, lz->currvalue, lz->tgtkey);
    else
        value = Py_BuildValue("O(OO)", Py_TYPE(lz),
            lz->it, lz->keyfunc);

    return value;
}

PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");

static PyObject *
groupby_setstate(groupbyobject *lz, PyObject *state)
{
    PyObject *currkey, *currvalue, *tgtkey;
    if (!PyTuple_Check(state)) {
        PyErr_SetString(PyExc_TypeError, "state is not a tuple");
        return NULL;
    }
    if (!PyArg_ParseTuple(state, "OOO", &currkey, &currvalue, &tgtkey)) {
        return NULL;
    }
    Py_INCREF(currkey);
    Py_XSETREF(lz->currkey, currkey);
    Py_INCREF(currvalue);
    Py_XSETREF(lz->currvalue, currvalue);
    Py_INCREF(tgtkey);
    Py_XSETREF(lz->tgtkey, tgtkey);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(setstate_doc, "Set state information for unpickling.");

static PyMethodDef groupby_methods[] = {
    {"__reduce__",      (PyCFunction)groupby_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)groupby_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}           /* sentinel */
};

PyDoc_STRVAR(groupby_doc,
"groupby(iterable, key=None) -> make an iterator that returns consecutive\n\
keys and groups from the iterable.  If the key function is not specified or\n\
is None, the element itself is used for grouping.\n");

static PyTypeObject groupby_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.groupby",                /* tp_name */
    sizeof(groupbyobject),              /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)groupby_dealloc,        /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    groupby_doc,                        /* tp_doc */
    (traverseproc)groupby_traverse,     /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)groupby_next,         /* tp_iternext */
    groupby_methods,                    /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    groupby_new,                        /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* _grouper object (internal) ************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *parent;
    PyObject *tgtkey;
} _grouperobject;

static PyTypeObject _grouper_type;

static PyObject *
_grouper_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *parent, *tgtkey;

    if (!PyArg_ParseTuple(args, "O!O", &groupby_type, &parent, &tgtkey))
        return NULL;

    return _grouper_create((groupbyobject*) parent, tgtkey);
}

static PyObject *
_grouper_create(groupbyobject *parent, PyObject *tgtkey)
{
    _grouperobject *igo;

    igo = PyObject_GC_New(_grouperobject, &_grouper_type);
    if (igo == NULL)
        return NULL;
    igo->parent = (PyObject *)parent;
    Py_INCREF(parent);
    igo->tgtkey = tgtkey;
    Py_INCREF(tgtkey);

    PyObject_GC_Track(igo);
    return (PyObject *)igo;
}

static void
_grouper_dealloc(_grouperobject *igo)
{
    PyObject_GC_UnTrack(igo);
    Py_DECREF(igo->parent);
    Py_DECREF(igo->tgtkey);
    PyObject_GC_Del(igo);
}

static int
_grouper_traverse(_grouperobject *igo, visitproc visit, void *arg)
{
    Py_VISIT(igo->parent);
    Py_VISIT(igo->tgtkey);
    return 0;
}

static PyObject *
_grouper_next(_grouperobject *igo)
{
    groupbyobject *gbo = (groupbyobject *)igo->parent;
    PyObject *r;
    int rcmp;

    if (gbo->currvalue == NULL) {
        if (groupby_step(gbo) < 0)
            return NULL;
    }

    assert(gbo->currkey != NULL);
    rcmp = PyObject_RichCompareBool(igo->tgtkey, gbo->currkey, Py_EQ);
    if (rcmp <= 0)
        /* got any error or current group is end */
        return NULL;

    r = gbo->currvalue;
    gbo->currvalue = NULL;
    Py_CLEAR(gbo->currkey);

    return r;
}

static PyObject *
_grouper_reduce(_grouperobject *lz)
{
    return Py_BuildValue("O(OO)", Py_TYPE(lz), lz->parent, lz->tgtkey);
}

static PyMethodDef _grouper_methods[] = {
    {"__reduce__",      (PyCFunction)_grouper_reduce,      METH_NOARGS,
     reduce_doc},
    {NULL,              NULL}   /* sentinel */
};


static PyTypeObject _grouper_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools._grouper",               /* tp_name */
    sizeof(_grouperobject),             /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)_grouper_dealloc,       /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,            /* tp_flags */
    0,                                  /* tp_doc */
    (traverseproc)_grouper_traverse,    /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)_grouper_next,        /* tp_iternext */
    _grouper_methods,                   /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    _grouper_new,                       /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* tee object and with supporting function and objects ***********************/

/* The teedataobject pre-allocates space for LINKCELLS number of objects.
   To help the object fit neatly inside cache lines (space for 16 to 32
   pointers), the value should be a multiple of 16 minus  space for
   the other structure members including PyHEAD overhead.  The larger the
   value, the less memory overhead per object and the less time spent
   allocating/deallocating new links.  The smaller the number, the less
   wasted space and the more rapid freeing of older data.
*/
#define LINKCELLS 57

typedef struct {
    PyObject_HEAD
    PyObject *it;
    int numread;                /* 0 <= numread <= LINKCELLS */
    PyObject *nextlink;
    PyObject *(values[LINKCELLS]);
} teedataobject;

typedef struct {
    PyObject_HEAD
    teedataobject *dataobj;
    int index;                  /* 0 <= index <= LINKCELLS */
    PyObject *weakreflist;
} teeobject;

static PyTypeObject teedataobject_type;

static PyObject *
teedataobject_newinternal(PyObject *it)
{
    teedataobject *tdo;

    tdo = PyObject_GC_New(teedataobject, &teedataobject_type);
    if (tdo == NULL)
        return NULL;

    tdo->numread = 0;
    tdo->nextlink = NULL;
    Py_INCREF(it);
    tdo->it = it;
    PyObject_GC_Track(tdo);
    return (PyObject *)tdo;
}

static PyObject *
teedataobject_jumplink(teedataobject *tdo)
{
    if (tdo->nextlink == NULL)
        tdo->nextlink = teedataobject_newinternal(tdo->it);
    Py_XINCREF(tdo->nextlink);
    return tdo->nextlink;
}

static PyObject *
teedataobject_getitem(teedataobject *tdo, int i)
{
    PyObject *value;

    assert(i < LINKCELLS);
    if (i < tdo->numread)
        value = tdo->values[i];
    else {
        /* this is the lead iterator, so fetch more data */
        assert(i == tdo->numread);
        value = PyIter_Next(tdo->it);
        if (value == NULL)
            return NULL;
        tdo->numread++;
        tdo->values[i] = value;
    }
    Py_INCREF(value);
    return value;
}

static int
teedataobject_traverse(teedataobject *tdo, visitproc visit, void * arg)
{
    int i;

    Py_VISIT(tdo->it);
    for (i = 0; i < tdo->numread; i++)
        Py_VISIT(tdo->values[i]);
    Py_VISIT(tdo->nextlink);
    return 0;
}

static void
teedataobject_safe_decref(PyObject *obj)
{
    while (obj && Py_TYPE(obj) == &teedataobject_type &&
           Py_REFCNT(obj) == 1) {
        PyObject *nextlink = ((teedataobject *)obj)->nextlink;
        ((teedataobject *)obj)->nextlink = NULL;
        Py_DECREF(obj);
        obj = nextlink;
    }
    Py_XDECREF(obj);
}

static int
teedataobject_clear(teedataobject *tdo)
{
    int i;
    PyObject *tmp;

    Py_CLEAR(tdo->it);
    for (i=0 ; i<tdo->numread ; i++)
        Py_CLEAR(tdo->values[i]);
    tmp = tdo->nextlink;
    tdo->nextlink = NULL;
    teedataobject_safe_decref(tmp);
    return 0;
}

static void
teedataobject_dealloc(teedataobject *tdo)
{
    PyObject_GC_UnTrack(tdo);
    teedataobject_clear(tdo);
    PyObject_GC_Del(tdo);
}

static PyObject *
teedataobject_reduce(teedataobject *tdo)
{
    int i;
    /* create a temporary list of already iterated values */
    PyObject *values = PyList_New(tdo->numread);

    if (!values)
        return NULL;
    for (i=0 ; i<tdo->numread ; i++) {
        Py_INCREF(tdo->values[i]);
        PyList_SET_ITEM(values, i, tdo->values[i]);
    }
    return Py_BuildValue("O(ONO)", Py_TYPE(tdo), tdo->it,
                         values,
                         tdo->nextlink ? tdo->nextlink : Py_None);
}

static PyTypeObject teedataobject_type;

static PyObject *
teedataobject_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    teedataobject *tdo;
    PyObject *it, *values, *next;
    Py_ssize_t i, len;

    assert(type == &teedataobject_type);
    if (!PyArg_ParseTuple(args, "OO!O", &it, &PyList_Type, &values, &next))
        return NULL;

    tdo = (teedataobject *)teedataobject_newinternal(it);
    if (!tdo)
        return NULL;

    len = PyList_GET_SIZE(values);
    if (len > LINKCELLS)
        goto err;
    for (i=0; i<len; i++) {
        tdo->values[i] = PyList_GET_ITEM(values, i);
        Py_INCREF(tdo->values[i]);
    }
    /* len <= LINKCELLS < INT_MAX */
    tdo->numread = Py_SAFE_DOWNCAST(len, Py_ssize_t, int);

    if (len == LINKCELLS) {
        if (next != Py_None) {
            if (Py_TYPE(next) != &teedataobject_type)
                goto err;
            assert(tdo->nextlink == NULL);
            Py_INCREF(next);
            tdo->nextlink = next;
        }
    } else {
        if (next != Py_None)
            goto err; /* shouldn't have a next if we are not full */
    }
    return (PyObject*)tdo;

err:
    Py_XDECREF(tdo);
    PyErr_SetString(PyExc_ValueError, "Invalid arguments");
    return NULL;
}

static PyMethodDef teedataobject_methods[] = {
    {"__reduce__",      (PyCFunction)teedataobject_reduce, METH_NOARGS,
     reduce_doc},
    {NULL,              NULL}           /* sentinel */
};

PyDoc_STRVAR(teedataobject_doc, "Data container common to multiple tee objects.");

static PyTypeObject teedataobject_type = {
    PyVarObject_HEAD_INIT(0, 0)                 /* Must fill in type value later */
    "itertools._tee_dataobject",                /* tp_name */
    sizeof(teedataobject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)teedataobject_dealloc,          /* tp_dealloc */
    0,                                          /* tp_print */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_reserved */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    teedataobject_doc,                          /* tp_doc */
    (traverseproc)teedataobject_traverse,       /* tp_traverse */
    (inquiry)teedataobject_clear,               /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    0,                                          /* tp_iter */
    0,                                          /* tp_iternext */
    teedataobject_methods,                      /* tp_methods */
    0,                                          /* tp_members */
    0,                                          /* tp_getset */
    0,                                          /* tp_base */
    0,                                          /* tp_dict */
    0,                                          /* tp_descr_get */
    0,                                          /* tp_descr_set */
    0,                                          /* tp_dictoffset */
    0,                                          /* tp_init */
    0,                                          /* tp_alloc */
    teedataobject_new,                          /* tp_new */
    PyObject_GC_Del,                            /* tp_free */
};


static PyTypeObject tee_type;

static PyObject *
tee_next(teeobject *to)
{
    PyObject *value, *link;

    if (to->index >= LINKCELLS) {
        link = teedataobject_jumplink(to->dataobj);
        if (link == NULL)
            return NULL;
        Py_SETREF(to->dataobj, (teedataobject *)link);
        to->index = 0;
    }
    value = teedataobject_getitem(to->dataobj, to->index);
    if (value == NULL)
        return NULL;
    to->index++;
    return value;
}

static int
tee_traverse(teeobject *to, visitproc visit, void *arg)
{
    Py_VISIT((PyObject *)to->dataobj);
    return 0;
}

static PyObject *
tee_copy(teeobject *to)
{
    teeobject *newto;

    newto = PyObject_GC_New(teeobject, &tee_type);
    if (newto == NULL)
        return NULL;
    Py_INCREF(to->dataobj);
    newto->dataobj = to->dataobj;
    newto->index = to->index;
    newto->weakreflist = NULL;
    PyObject_GC_Track(newto);
    return (PyObject *)newto;
}

PyDoc_STRVAR(teecopy_doc, "Returns an independent iterator.");

static PyObject *
tee_fromiterable(PyObject *iterable)
{
    teeobject *to;
    PyObject *it = NULL;

    it = PyObject_GetIter(iterable);
    if (it == NULL)
        return NULL;
    if (PyObject_TypeCheck(it, &tee_type)) {
        to = (teeobject *)tee_copy((teeobject *)it);
        goto done;
    }

    to = PyObject_GC_New(teeobject, &tee_type);
    if (to == NULL)
        goto done;
    to->dataobj = (teedataobject *)teedataobject_newinternal(it);
    if (!to->dataobj) {
        PyObject_GC_Del(to);
        to = NULL;
        goto done;
    }

    to->index = 0;
    to->weakreflist = NULL;
    PyObject_GC_Track(to);
done:
    Py_XDECREF(it);
    return (PyObject *)to;
}

static PyObject *
tee_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    PyObject *iterable;

    if (!PyArg_UnpackTuple(args, "_tee", 1, 1, &iterable))
        return NULL;
    return tee_fromiterable(iterable);
}

static int
tee_clear(teeobject *to)
{
    if (to->weakreflist != NULL)
        PyObject_ClearWeakRefs((PyObject *) to);
    Py_CLEAR(to->dataobj);
    return 0;
}

static void
tee_dealloc(teeobject *to)
{
    PyObject_GC_UnTrack(to);
    tee_clear(to);
    PyObject_GC_Del(to);
}

static PyObject *
tee_reduce(teeobject *to)
{
    return Py_BuildValue("O(())(Oi)", Py_TYPE(to), to->dataobj, to->index);
}

static PyObject *
tee_setstate(teeobject *to, PyObject *state)
{
    teedataobject *tdo;
    int index;
    if (!PyTuple_Check(state)) {
        PyErr_SetString(PyExc_TypeError, "state is not a tuple");
        return NULL;
    }
    if (!PyArg_ParseTuple(state, "O!i", &teedataobject_type, &tdo, &index)) {
        return NULL;
    }
    if (index < 0 || index > LINKCELLS) {
        PyErr_SetString(PyExc_ValueError, "Index out of range");
        return NULL;
    }
    Py_INCREF(tdo);
    Py_XSETREF(to->dataobj, tdo);
    to->index = index;
    Py_RETURN_NONE;
}

PyDoc_STRVAR(teeobject_doc,
"Iterator wrapped to make it copyable");

static PyMethodDef tee_methods[] = {
    {"__copy__",        (PyCFunction)tee_copy,     METH_NOARGS, teecopy_doc},
    {"__reduce__",      (PyCFunction)tee_reduce,   METH_NOARGS, reduce_doc},
    {"__setstate__",    (PyCFunction)tee_setstate, METH_O,      setstate_doc},
    {NULL,              NULL}           /* sentinel */
};

static PyTypeObject tee_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools._tee",                   /* tp_name */
    sizeof(teeobject),                  /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)tee_dealloc,            /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    0,                                  /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,            /* tp_flags */
    teeobject_doc,                      /* tp_doc */
    (traverseproc)tee_traverse,         /* tp_traverse */
    (inquiry)tee_clear,                 /* tp_clear */
    0,                                  /* tp_richcompare */
    offsetof(teeobject, weakreflist),   /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)tee_next,             /* tp_iternext */
    tee_methods,                        /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    tee_new,                            /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};

static PyObject *
tee(PyObject *self, PyObject *args)
{
    Py_ssize_t i, n=2;
    PyObject *it, *iterable, *copyable, *result;
    _Py_IDENTIFIER(__copy__);

    if (!PyArg_ParseTuple(args, "O|n", &iterable, &n))
        return NULL;
    if (n < 0) {
        PyErr_SetString(PyExc_ValueError, "n must be >= 0");
        return NULL;
    }
    result = PyTuple_New(n);
    if (result == NULL)
        return NULL;
    if (n == 0)
        return result;
    it = PyObject_GetIter(iterable);
    if (it == NULL) {
        Py_DECREF(result);
        return NULL;
    }
    if (!_PyObject_HasAttrId(it, &PyId___copy__)) {
        copyable = tee_fromiterable(it);
        Py_DECREF(it);
        if (copyable == NULL) {
            Py_DECREF(result);
            return NULL;
        }
    } else
        copyable = it;
    PyTuple_SET_ITEM(result, 0, copyable);
    for (i=1 ; i<n ; i++) {

        copyable = _PyObject_CallMethodId(copyable, &PyId___copy__, NULL);
        if (copyable == NULL) {
            Py_DECREF(result);
            return NULL;
        }
        PyTuple_SET_ITEM(result, i, copyable);
    }
    return result;
}

PyDoc_STRVAR(tee_doc,
"tee(iterable, n=2) --> tuple of n independent iterators.");


/* cycle object **************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *it;
    PyObject *saved;
    Py_ssize_t index;
    int firstpass;
} cycleobject;

static PyTypeObject cycle_type;

static PyObject *
cycle_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *it;
    PyObject *iterable;
    PyObject *saved;
    cycleobject *lz;

    if (type == &cycle_type && !_PyArg_NoKeywords("cycle()", kwds))
        return NULL;

    if (!PyArg_UnpackTuple(args, "cycle", 1, 1, &iterable))
        return NULL;

    /* Get iterator. */
    it = PyObject_GetIter(iterable);
    if (it == NULL)
        return NULL;

    saved = PyList_New(0);
    if (saved == NULL) {
        Py_DECREF(it);
        return NULL;
    }

    /* create cycleobject structure */
    lz = (cycleobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(it);
        Py_DECREF(saved);
        return NULL;
    }
    lz->it = it;
    lz->saved = saved;
    lz->index = 0;
    lz->firstpass = 0;

    return (PyObject *)lz;
}

static void
cycle_dealloc(cycleobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->it);
    Py_XDECREF(lz->saved);
    Py_TYPE(lz)->tp_free(lz);
}

static int
cycle_traverse(cycleobject *lz, visitproc visit, void *arg)
{
    if (lz->it)
        Py_VISIT(lz->it);
    Py_VISIT(lz->saved);
    return 0;
}

static PyObject *
cycle_next(cycleobject *lz)
{
    PyObject *item;

    if (lz->it != NULL) {
        item = PyIter_Next(lz->it);
        if (item != NULL) {
            if (lz->firstpass)
                return item;
            if (PyList_Append(lz->saved, item)) {
                Py_DECREF(item);
                return NULL;
            }
            return item;
        }
        /* Note:  StopIteration is already cleared by PyIter_Next() */
        if (PyErr_Occurred())
            return NULL;
        Py_CLEAR(lz->it);
    }
    if (Py_SIZE(lz->saved) == 0)
        return NULL;
    item = PyList_GET_ITEM(lz->saved, lz->index);
    lz->index++;
    if (lz->index >= Py_SIZE(lz->saved))
        lz->index = 0;
    Py_INCREF(item);
    return item;
}

static PyObject *
cycle_reduce(cycleobject *lz)
{
    /* Create a new cycle with the iterator tuple, then set the saved state */
    if (lz->it == NULL) {
        PyObject *it = PyObject_GetIter(lz->saved);
        if (it == NULL)
            return NULL;
        if (lz->index != 0) {
            _Py_IDENTIFIER(__setstate__);
            PyObject *res = _PyObject_CallMethodId(it, &PyId___setstate__,
                                                   "n", lz->index);
            if (res == NULL) {
                Py_DECREF(it);
                return NULL;
            }
            Py_DECREF(res);
        }
        return Py_BuildValue("O(N)(Oi)", Py_TYPE(lz), it, lz->saved, 1);
    }
    return Py_BuildValue("O(O)(Oi)", Py_TYPE(lz), lz->it, lz->saved,
                         lz->firstpass);
}

static PyObject *
cycle_setstate(cycleobject *lz, PyObject *state)
{
    PyObject *saved=NULL;
    int firstpass;
    if (!PyTuple_Check(state)) {
        PyErr_SetString(PyExc_TypeError, "state is not a tuple");
        return NULL;
    }
    if (!PyArg_ParseTuple(state, "O!i", &PyList_Type, &saved, &firstpass)) {
        return NULL;
    }
    Py_INCREF(saved);
    Py_XSETREF(lz->saved, saved);
    lz->firstpass = firstpass != 0;
    lz->index = 0;
    Py_RETURN_NONE;
}

static PyMethodDef cycle_methods[] = {
    {"__reduce__",      (PyCFunction)cycle_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)cycle_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(cycle_doc,
"cycle(iterable) --> cycle object\n\
\n\
Return elements from the iterable until it is exhausted.\n\
Then repeat the sequence indefinitely.");

static PyTypeObject cycle_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.cycle",                  /* tp_name */
    sizeof(cycleobject),                /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)cycle_dealloc,          /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    cycle_doc,                          /* tp_doc */
    (traverseproc)cycle_traverse,       /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)cycle_next,           /* tp_iternext */
    cycle_methods,                      /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    cycle_new,                          /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* dropwhile object **********************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *func;
    PyObject *it;
    long start;
} dropwhileobject;

static PyTypeObject dropwhile_type;

static PyObject *
dropwhile_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *func, *seq;
    PyObject *it;
    dropwhileobject *lz;

    if (type == &dropwhile_type && !_PyArg_NoKeywords("dropwhile()", kwds))
        return NULL;

    if (!PyArg_UnpackTuple(args, "dropwhile", 2, 2, &func, &seq))
        return NULL;

    /* Get iterator. */
    it = PyObject_GetIter(seq);
    if (it == NULL)
        return NULL;

    /* create dropwhileobject structure */
    lz = (dropwhileobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(it);
        return NULL;
    }
    Py_INCREF(func);
    lz->func = func;
    lz->it = it;
    lz->start = 0;

    return (PyObject *)lz;
}

static void
dropwhile_dealloc(dropwhileobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->func);
    Py_XDECREF(lz->it);
    Py_TYPE(lz)->tp_free(lz);
}

static int
dropwhile_traverse(dropwhileobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->it);
    Py_VISIT(lz->func);
    return 0;
}

static PyObject *
dropwhile_next(dropwhileobject *lz)
{
    PyObject *item, *good;
    PyObject *it = lz->it;
    long ok;
    PyObject *(*iternext)(PyObject *);

    iternext = *Py_TYPE(it)->tp_iternext;
    for (;;) {
        item = iternext(it);
        if (item == NULL)
            return NULL;
        if (lz->start == 1)
            return item;

        good = PyObject_CallFunctionObjArgs(lz->func, item, NULL);
        if (good == NULL) {
            Py_DECREF(item);
            return NULL;
        }
        ok = PyObject_IsTrue(good);
        Py_DECREF(good);
        if (ok == 0) {
            lz->start = 1;
            return item;
        }
        Py_DECREF(item);
        if (ok < 0)
            return NULL;
    }
}

static PyObject *
dropwhile_reduce(dropwhileobject *lz)
{
    return Py_BuildValue("O(OO)l", Py_TYPE(lz), lz->func, lz->it, lz->start);
}

static PyObject *
dropwhile_setstate(dropwhileobject *lz, PyObject *state)
{
    int start = PyObject_IsTrue(state);
    if (start < 0)
        return NULL;
    lz->start = start;
    Py_RETURN_NONE;
}

static PyMethodDef dropwhile_methods[] = {
    {"__reduce__",      (PyCFunction)dropwhile_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)dropwhile_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(dropwhile_doc,
"dropwhile(predicate, iterable) --> dropwhile object\n\
\n\
Drop items from the iterable while predicate(item) is true.\n\
Afterwards, return every element until the iterable is exhausted.");

static PyTypeObject dropwhile_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.dropwhile",              /* tp_name */
    sizeof(dropwhileobject),            /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)dropwhile_dealloc,      /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    dropwhile_doc,                      /* tp_doc */
    (traverseproc)dropwhile_traverse,   /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)dropwhile_next,       /* tp_iternext */
    dropwhile_methods,                  /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    dropwhile_new,                      /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* takewhile object **********************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *func;
    PyObject *it;
    long stop;
} takewhileobject;

static PyTypeObject takewhile_type;

static PyObject *
takewhile_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *func, *seq;
    PyObject *it;
    takewhileobject *lz;

    if (type == &takewhile_type && !_PyArg_NoKeywords("takewhile()", kwds))
        return NULL;

    if (!PyArg_UnpackTuple(args, "takewhile", 2, 2, &func, &seq))
        return NULL;

    /* Get iterator. */
    it = PyObject_GetIter(seq);
    if (it == NULL)
        return NULL;

    /* create takewhileobject structure */
    lz = (takewhileobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(it);
        return NULL;
    }
    Py_INCREF(func);
    lz->func = func;
    lz->it = it;
    lz->stop = 0;

    return (PyObject *)lz;
}

static void
takewhile_dealloc(takewhileobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->func);
    Py_XDECREF(lz->it);
    Py_TYPE(lz)->tp_free(lz);
}

static int
takewhile_traverse(takewhileobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->it);
    Py_VISIT(lz->func);
    return 0;
}

static PyObject *
takewhile_next(takewhileobject *lz)
{
    PyObject *item, *good;
    PyObject *it = lz->it;
    long ok;

    if (lz->stop == 1)
        return NULL;

    item = (*Py_TYPE(it)->tp_iternext)(it);
    if (item == NULL)
        return NULL;

    good = PyObject_CallFunctionObjArgs(lz->func, item, NULL);
    if (good == NULL) {
        Py_DECREF(item);
        return NULL;
    }
    ok = PyObject_IsTrue(good);
    Py_DECREF(good);
    if (ok > 0)
        return item;
    Py_DECREF(item);
    if (ok == 0)
        lz->stop = 1;
    return NULL;
}

static PyObject *
takewhile_reduce(takewhileobject *lz)
{
    return Py_BuildValue("O(OO)l", Py_TYPE(lz), lz->func, lz->it, lz->stop);
}

static PyObject *
takewhile_reduce_setstate(takewhileobject *lz, PyObject *state)
{
    int stop = PyObject_IsTrue(state);

    if (stop < 0)
        return NULL;
    lz->stop = stop;
    Py_RETURN_NONE;
}

static PyMethodDef takewhile_reduce_methods[] = {
    {"__reduce__",      (PyCFunction)takewhile_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)takewhile_reduce_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}   /* sentinel */
};
PyDoc_STRVAR(takewhile_doc,
"takewhile(predicate, iterable) --> takewhile object\n\
\n\
Return successive entries from an iterable as long as the \n\
predicate evaluates to true for each entry.");

static PyTypeObject takewhile_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.takewhile",              /* tp_name */
    sizeof(takewhileobject),            /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)takewhile_dealloc,      /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    takewhile_doc,                      /* tp_doc */
    (traverseproc)takewhile_traverse,   /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)takewhile_next,       /* tp_iternext */
    takewhile_reduce_methods,           /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    takewhile_new,                      /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* islice object *************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *it;
    Py_ssize_t next;
    Py_ssize_t stop;
    Py_ssize_t step;
    Py_ssize_t cnt;
} isliceobject;

static PyTypeObject islice_type;

static PyObject *
islice_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *seq;
    Py_ssize_t start=0, stop=-1, step=1;
    PyObject *it, *a1=NULL, *a2=NULL, *a3=NULL;
    Py_ssize_t numargs;
    isliceobject *lz;

    if (type == &islice_type && !_PyArg_NoKeywords("islice()", kwds))
        return NULL;

    if (!PyArg_UnpackTuple(args, "islice", 2, 4, &seq, &a1, &a2, &a3))
        return NULL;

    numargs = PyTuple_Size(args);
    if (numargs == 2) {
        if (a1 != Py_None) {
            stop = PyLong_AsSsize_t(a1);
            if (stop == -1) {
                if (PyErr_Occurred())
                    PyErr_Clear();
                PyErr_SetString(PyExc_ValueError,
                   "Stop argument for islice() must be None or "
                   "an integer: 0 <= x <= sys.maxsize.");
                return NULL;
            }
        }
    } else {
        if (a1 != Py_None)
            start = PyLong_AsSsize_t(a1);
        if (start == -1 && PyErr_Occurred())
            PyErr_Clear();
        if (a2 != Py_None) {
            stop = PyLong_AsSsize_t(a2);
            if (stop == -1) {
                if (PyErr_Occurred())
                    PyErr_Clear();
                PyErr_SetString(PyExc_ValueError,
                   "Stop argument for islice() must be None or "
                   "an integer: 0 <= x <= sys.maxsize.");
                return NULL;
            }
        }
    }
    if (start<0 || stop<-1) {
        PyErr_SetString(PyExc_ValueError,
           "Indices for islice() must be None or "
           "an integer: 0 <= x <= sys.maxsize.");
        return NULL;
    }

    if (a3 != NULL) {
        if (a3 != Py_None)
            step = PyLong_AsSsize_t(a3);
        if (step == -1 && PyErr_Occurred())
            PyErr_Clear();
    }
    if (step<1) {
        PyErr_SetString(PyExc_ValueError,
           "Step for islice() must be a positive integer or None.");
        return NULL;
    }

    /* Get iterator. */
    it = PyObject_GetIter(seq);
    if (it == NULL)
        return NULL;

    /* create isliceobject structure */
    lz = (isliceobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(it);
        return NULL;
    }
    lz->it = it;
    lz->next = start;
    lz->stop = stop;
    lz->step = step;
    lz->cnt = 0L;

    return (PyObject *)lz;
}

static void
islice_dealloc(isliceobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->it);
    Py_TYPE(lz)->tp_free(lz);
}

static int
islice_traverse(isliceobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->it);
    return 0;
}

static PyObject *
islice_next(isliceobject *lz)
{
    PyObject *item;
    PyObject *it = lz->it;
    Py_ssize_t stop = lz->stop;
    Py_ssize_t oldnext;
    PyObject *(*iternext)(PyObject *);

    if (it == NULL)
        return NULL;

    iternext = *Py_TYPE(it)->tp_iternext;
    while (lz->cnt < lz->next) {
        item = iternext(it);
        if (item == NULL)
            goto empty;
        Py_DECREF(item);
        lz->cnt++;
    }
    if (stop != -1 && lz->cnt >= stop)
        goto empty;
    item = iternext(it);
    if (item == NULL)
        goto empty;
    lz->cnt++;
    oldnext = lz->next;
    /* The (size_t) cast below avoids the danger of undefined
       behaviour from signed integer overflow. */
    lz->next += (size_t)lz->step;
    if (lz->next < oldnext || (stop != -1 && lz->next > stop))
        lz->next = stop;
    return item;

empty:
    Py_CLEAR(lz->it);
    return NULL;
}

static PyObject *
islice_reduce(isliceobject *lz)
{
    /* When unpickled, generate a new object with the same bounds,
     * then 'setstate' with the next and count
     */
    PyObject *stop;

    if (lz->it == NULL) {
        PyObject *empty_list;
        PyObject *empty_it;
        empty_list = PyList_New(0);
        if (empty_list == NULL)
            return NULL;
        empty_it = PyObject_GetIter(empty_list);
        Py_DECREF(empty_list);
        if (empty_it == NULL)
            return NULL;
        return Py_BuildValue("O(Nn)n", Py_TYPE(lz), empty_it, 0, 0);
    }
    if (lz->stop == -1) {
        stop = Py_None;
        Py_INCREF(stop);
    } else {
        stop = PyLong_FromSsize_t(lz->stop);
        if (stop == NULL)
            return NULL;
    }
    return Py_BuildValue("O(OnNn)n", Py_TYPE(lz),
        lz->it, lz->next, stop, lz->step,
        lz->cnt);
}

static PyObject *
islice_setstate(isliceobject *lz, PyObject *state)
{
    Py_ssize_t cnt = PyLong_AsSsize_t(state);

    if (cnt == -1 && PyErr_Occurred())
        return NULL;
    lz->cnt = cnt;
    Py_RETURN_NONE;
}

static PyMethodDef islice_methods[] = {
    {"__reduce__",      (PyCFunction)islice_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)islice_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(islice_doc,
"islice(iterable, stop) --> islice object\n\
islice(iterable, start, stop[, step]) --> islice object\n\
\n\
Return an iterator whose next() method returns selected values from an\n\
iterable.  If start is specified, will skip all preceding elements;\n\
otherwise, start defaults to zero.  Step defaults to one.  If\n\
specified as another value, step determines how many values are \n\
skipped between successive calls.  Works like a slice() on a list\n\
but returns an iterator.");

static PyTypeObject islice_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.islice",                 /* tp_name */
    sizeof(isliceobject),               /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)islice_dealloc,         /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    islice_doc,                         /* tp_doc */
    (traverseproc)islice_traverse,      /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)islice_next,          /* tp_iternext */
    islice_methods,                     /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    islice_new,                         /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* starmap object ************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *func;
    PyObject *it;
} starmapobject;

static PyTypeObject starmap_type;

static PyObject *
starmap_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *func, *seq;
    PyObject *it;
    starmapobject *lz;

    if (type == &starmap_type && !_PyArg_NoKeywords("starmap()", kwds))
        return NULL;

    if (!PyArg_UnpackTuple(args, "starmap", 2, 2, &func, &seq))
        return NULL;

    /* Get iterator. */
    it = PyObject_GetIter(seq);
    if (it == NULL)
        return NULL;

    /* create starmapobject structure */
    lz = (starmapobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(it);
        return NULL;
    }
    Py_INCREF(func);
    lz->func = func;
    lz->it = it;

    return (PyObject *)lz;
}

static void
starmap_dealloc(starmapobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->func);
    Py_XDECREF(lz->it);
    Py_TYPE(lz)->tp_free(lz);
}

static int
starmap_traverse(starmapobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->it);
    Py_VISIT(lz->func);
    return 0;
}

static PyObject *
starmap_next(starmapobject *lz)
{
    PyObject *args;
    PyObject *result;
    PyObject *it = lz->it;

    args = (*Py_TYPE(it)->tp_iternext)(it);
    if (args == NULL)
        return NULL;
    if (!PyTuple_CheckExact(args)) {
        PyObject *newargs = PySequence_Tuple(args);
        Py_DECREF(args);
        if (newargs == NULL)
            return NULL;
        args = newargs;
    }
    result = PyObject_Call(lz->func, args, NULL);
    Py_DECREF(args);
    return result;
}

static PyObject *
starmap_reduce(starmapobject *lz)
{
    /* Just pickle the iterator */
    return Py_BuildValue("O(OO)", Py_TYPE(lz), lz->func, lz->it);
}

static PyMethodDef starmap_methods[] = {
    {"__reduce__",      (PyCFunction)starmap_reduce,      METH_NOARGS,
     reduce_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(starmap_doc,
"starmap(function, sequence) --> starmap object\n\
\n\
Return an iterator whose values are returned from the function evaluated\n\
with an argument tuple taken from the given sequence.");

static PyTypeObject starmap_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.starmap",                /* tp_name */
    sizeof(starmapobject),              /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)starmap_dealloc,        /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    starmap_doc,                        /* tp_doc */
    (traverseproc)starmap_traverse,     /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)starmap_next,         /* tp_iternext */
    starmap_methods,                    /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    starmap_new,                        /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* chain object **************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *source;                   /* Iterator over input iterables */
    PyObject *active;                   /* Currently running input iterator */
} chainobject;

static PyTypeObject chain_type;

static PyObject *
chain_new_internal(PyTypeObject *type, PyObject *source)
{
    chainobject *lz;

    lz = (chainobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(source);
        return NULL;
    }

    lz->source = source;
    lz->active = NULL;
    return (PyObject *)lz;
}

static PyObject *
chain_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *source;

    if (type == &chain_type && !_PyArg_NoKeywords("chain()", kwds))
        return NULL;

    source = PyObject_GetIter(args);
    if (source == NULL)
        return NULL;

    return chain_new_internal(type, source);
}

static PyObject *
chain_new_from_iterable(PyTypeObject *type, PyObject *arg)
{
    PyObject *source;

    source = PyObject_GetIter(arg);
    if (source == NULL)
        return NULL;

    return chain_new_internal(type, source);
}

static void
chain_dealloc(chainobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->active);
    Py_XDECREF(lz->source);
    Py_TYPE(lz)->tp_free(lz);
}

static int
chain_traverse(chainobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->source);
    Py_VISIT(lz->active);
    return 0;
}

static PyObject *
chain_next(chainobject *lz)
{
    PyObject *item;

    /* lz->source is the iterator of iterables. If it's NULL, we've already
     * consumed them all. lz->active is the current iterator. If it's NULL,
     * we should grab a new one from lz->source. */
    while (lz->source != NULL) {
        if (lz->active == NULL) {
            PyObject *iterable = PyIter_Next(lz->source);
            if (iterable == NULL) {
                Py_CLEAR(lz->source);
                return NULL;            /* no more input sources */
            }
            lz->active = PyObject_GetIter(iterable);
            Py_DECREF(iterable);
            if (lz->active == NULL) {
                Py_CLEAR(lz->source);
                return NULL;            /* input not iterable */
            }
        }
        item = (*Py_TYPE(lz->active)->tp_iternext)(lz->active);
        if (item != NULL)
            return item;
        if (PyErr_Occurred()) {
            if (PyErr_ExceptionMatches(PyExc_StopIteration))
                PyErr_Clear();
            else
                return NULL;            /* input raised an exception */
        }
        /* lz->active is consumed, try with the next iterable. */
        Py_CLEAR(lz->active);
    }
    /* Everything had been consumed already. */
    return NULL;
}

static PyObject *
chain_reduce(chainobject *lz)
{
    if (lz->source) {
        /* we can't pickle function objects (itertools.from_iterable) so
         * we must use setstate to replace the iterable.  One day we
         * will fix pickling of functions
         */
        if (lz->active) {
            return Py_BuildValue("O()(OO)", Py_TYPE(lz), lz->source, lz->active);
        } else {
            return Py_BuildValue("O()(O)", Py_TYPE(lz), lz->source);
        }
    } else {
        return Py_BuildValue("O()", Py_TYPE(lz)); /* exhausted */
    }
    return NULL;
}

static PyObject *
chain_setstate(chainobject *lz, PyObject *state)
{
    PyObject *source, *active=NULL;

    if (!PyTuple_Check(state)) {
        PyErr_SetString(PyExc_TypeError, "state is not a tuple");
        return NULL;
    }
    if (!PyArg_ParseTuple(state, "O|O", &source, &active)) {
        return NULL;
    }
    if (!PyIter_Check(source) || (active != NULL && !PyIter_Check(active))) {
        PyErr_SetString(PyExc_TypeError, "Arguments must be iterators.");
        return NULL;
    }

    Py_INCREF(source);
    Py_XSETREF(lz->source, source);
    Py_XINCREF(active);
    Py_XSETREF(lz->active, active);
    Py_RETURN_NONE;
}

PyDoc_STRVAR(chain_doc,
"chain(*iterables) --> chain object\n\
\n\
Return a chain object whose .__next__() method returns elements from the\n\
first iterable until it is exhausted, then elements from the next\n\
iterable, until all of the iterables are exhausted.");

PyDoc_STRVAR(chain_from_iterable_doc,
"chain.from_iterable(iterable) --> chain object\n\
\n\
Alternate chain() constructor taking a single iterable argument\n\
that evaluates lazily.");

static PyMethodDef chain_methods[] = {
    {"from_iterable", (PyCFunction) chain_new_from_iterable, METH_O | METH_CLASS,
     chain_from_iterable_doc},
    {"__reduce__",      (PyCFunction)chain_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)chain_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}           /* sentinel */
};

static PyTypeObject chain_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.chain",                  /* tp_name */
    sizeof(chainobject),                /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)chain_dealloc,          /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    chain_doc,                          /* tp_doc */
    (traverseproc)chain_traverse,       /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)chain_next,           /* tp_iternext */
    chain_methods,                      /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    chain_new,                          /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* product object ************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *pools;        /* tuple of pool tuples */
    Py_ssize_t *indices;    /* one index per pool */
    PyObject *result;       /* most recently returned result tuple */
    int stopped;            /* set to 1 when the iterator is exhausted */
} productobject;

static PyTypeObject product_type;

static PyObject *
product_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    productobject *lz;
    Py_ssize_t nargs, npools, repeat=1;
    PyObject *pools = NULL;
    Py_ssize_t *indices = NULL;
    Py_ssize_t i;

    if (kwds != NULL) {
        char *kwlist[] = {"repeat", 0};
        PyObject *tmpargs = PyTuple_New(0);
        if (tmpargs == NULL)
            return NULL;
        if (!PyArg_ParseTupleAndKeywords(tmpargs, kwds, "|n:product",
                                         kwlist, &repeat)) {
            Py_DECREF(tmpargs);
            return NULL;
        }
        Py_DECREF(tmpargs);
        if (repeat < 0) {
            PyErr_SetString(PyExc_ValueError,
                            "repeat argument cannot be negative");
            return NULL;
        }
    }

    assert(PyTuple_CheckExact(args));
    if (repeat == 0) {
        nargs = 0;
    } else {
        nargs = PyTuple_GET_SIZE(args);
        if ((size_t)nargs > PY_SSIZE_T_MAX/sizeof(Py_ssize_t)/repeat) {
            PyErr_SetString(PyExc_OverflowError, "repeat argument too large");
            return NULL;
        }
    }
    npools = nargs * repeat;

    indices = PyMem_New(Py_ssize_t, npools);
    if (indices == NULL) {
        PyErr_NoMemory();
        goto error;
    }

    pools = PyTuple_New(npools);
    if (pools == NULL)
        goto error;

    for (i=0; i < nargs ; ++i) {
        PyObject *item = PyTuple_GET_ITEM(args, i);
        PyObject *pool = PySequence_Tuple(item);
        if (pool == NULL)
            goto error;
        PyTuple_SET_ITEM(pools, i, pool);
        indices[i] = 0;
    }
    for ( ; i < npools; ++i) {
        PyObject *pool = PyTuple_GET_ITEM(pools, i - nargs);
        Py_INCREF(pool);
        PyTuple_SET_ITEM(pools, i, pool);
        indices[i] = 0;
    }

    /* create productobject structure */
    lz = (productobject *)type->tp_alloc(type, 0);
    if (lz == NULL)
        goto error;

    lz->pools = pools;
    lz->indices = indices;
    lz->result = NULL;
    lz->stopped = 0;

    return (PyObject *)lz;

error:
    if (indices != NULL)
        PyMem_Free(indices);
    Py_XDECREF(pools);
    return NULL;
}

static void
product_dealloc(productobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->pools);
    Py_XDECREF(lz->result);
    if (lz->indices != NULL)
        PyMem_Free(lz->indices);
    Py_TYPE(lz)->tp_free(lz);
}

static PyObject *
product_sizeof(productobject *lz, void *unused)
{
    Py_ssize_t res;

    res = _PyObject_SIZE(Py_TYPE(lz));
    res += PyTuple_GET_SIZE(lz->pools) * sizeof(Py_ssize_t);
    return PyLong_FromSsize_t(res);
}

PyDoc_STRVAR(sizeof_doc, "Returns size in memory, in bytes.");

static int
product_traverse(productobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->pools);
    Py_VISIT(lz->result);
    return 0;
}

static PyObject *
product_next(productobject *lz)
{
    PyObject *pool;
    PyObject *elem;
    PyObject *oldelem;
    PyObject *pools = lz->pools;
    PyObject *result = lz->result;
    Py_ssize_t npools = PyTuple_GET_SIZE(pools);
    Py_ssize_t i;

    if (lz->stopped)
        return NULL;

    if (result == NULL) {
        /* On the first pass, return an initial tuple filled with the
           first element from each pool. */
        result = PyTuple_New(npools);
        if (result == NULL)
            goto empty;
        lz->result = result;
        for (i=0; i < npools; i++) {
            pool = PyTuple_GET_ITEM(pools, i);
            if (PyTuple_GET_SIZE(pool) == 0)
                goto empty;
            elem = PyTuple_GET_ITEM(pool, 0);
            Py_INCREF(elem);
            PyTuple_SET_ITEM(result, i, elem);
        }
    } else {
        Py_ssize_t *indices = lz->indices;

        /* Copy the previous result tuple or re-use it if available */
        if (Py_REFCNT(result) > 1) {
            PyObject *old_result = result;
            result = PyTuple_New(npools);
            if (result == NULL)
                goto empty;
            lz->result = result;
            for (i=0; i < npools; i++) {
                elem = PyTuple_GET_ITEM(old_result, i);
                Py_INCREF(elem);
                PyTuple_SET_ITEM(result, i, elem);
            }
            Py_DECREF(old_result);
        }
        /* Now, we've got the only copy so we can update it in-place */
        assert (npools==0 || Py_REFCNT(result) == 1);

        /* Update the pool indices right-to-left.  Only advance to the
           next pool when the previous one rolls-over */
        for (i=npools-1 ; i >= 0 ; i--) {
            pool = PyTuple_GET_ITEM(pools, i);
            indices[i]++;
            if (indices[i] == PyTuple_GET_SIZE(pool)) {
                /* Roll-over and advance to next pool */
                indices[i] = 0;
                elem = PyTuple_GET_ITEM(pool, 0);
                Py_INCREF(elem);
                oldelem = PyTuple_GET_ITEM(result, i);
                PyTuple_SET_ITEM(result, i, elem);
                Py_DECREF(oldelem);
            } else {
                /* No rollover. Just increment and stop here. */
                elem = PyTuple_GET_ITEM(pool, indices[i]);
                Py_INCREF(elem);
                oldelem = PyTuple_GET_ITEM(result, i);
                PyTuple_SET_ITEM(result, i, elem);
                Py_DECREF(oldelem);
                break;
            }
        }

        /* If i is negative, then the indices have all rolled-over
           and we're done. */
        if (i < 0)
            goto empty;
    }

    Py_INCREF(result);
    return result;

empty:
    lz->stopped = 1;
    return NULL;
}

static PyObject *
product_reduce(productobject *lz)
{
    if (lz->stopped) {
        return Py_BuildValue("O(())", Py_TYPE(lz));
    } else if (lz->result == NULL) {
        return Py_BuildValue("OO", Py_TYPE(lz), lz->pools);
    } else {
        PyObject *indices;
        Py_ssize_t n, i;

        /* we must pickle the indices use them for setstate, and
         * additionally indicate that the iterator has started
         */
        n = PyTuple_GET_SIZE(lz->pools);
        indices = PyTuple_New(n);
        if (indices == NULL)
            return NULL;
        for (i=0; i<n; i++){
            PyObject* index = PyLong_FromSsize_t(lz->indices[i]);
            if (!index) {
                Py_DECREF(indices);
                return NULL;
            }
            PyTuple_SET_ITEM(indices, i, index);
        }
        return Py_BuildValue("OON", Py_TYPE(lz), lz->pools, indices);
    }
}

static PyObject *
product_setstate(productobject *lz, PyObject *state)
{
    PyObject *result;
    Py_ssize_t n, i;

    n = PyTuple_GET_SIZE(lz->pools);
    if (!PyTuple_Check(state) || PyTuple_GET_SIZE(state) != n) {
        PyErr_SetString(PyExc_ValueError, "invalid arguments");
        return NULL;
    }
    for (i=0; i<n; i++)
    {
        PyObject* indexObject = PyTuple_GET_ITEM(state, i);
        Py_ssize_t index = PyLong_AsSsize_t(indexObject);
        PyObject* pool;
        Py_ssize_t poolsize;
        if (index < 0 && PyErr_Occurred())
            return NULL; /* not an integer */
        pool = PyTuple_GET_ITEM(lz->pools, i);
        poolsize = PyTuple_GET_SIZE(pool);
        if (poolsize == 0) {
            lz->stopped = 1;
            Py_RETURN_NONE;
        }
        /* clamp the index */
        if (index < 0)
            index = 0;
        else if (index > poolsize-1)
            index = poolsize-1;
        lz->indices[i] = index;
    }

    result = PyTuple_New(n);
    if (!result)
        return NULL;
    for (i=0; i<n; i++) {
        PyObject *pool = PyTuple_GET_ITEM(lz->pools, i);
        PyObject *element = PyTuple_GET_ITEM(pool, lz->indices[i]);
        Py_INCREF(element);
        PyTuple_SET_ITEM(result, i, element);
    }
    Py_XSETREF(lz->result, result);
    Py_RETURN_NONE;
}

static PyMethodDef product_methods[] = {
    {"__reduce__",      (PyCFunction)product_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)product_setstate,    METH_O,
     setstate_doc},
    {"__sizeof__",      (PyCFunction)product_sizeof,      METH_NOARGS,
     sizeof_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(product_doc,
"product(*iterables, repeat=1) --> product object\n\
\n\
Cartesian product of input iterables.  Equivalent to nested for-loops.\n\n\
For example, product(A, B) returns the same as:  ((x,y) for x in A for y in B).\n\
The leftmost iterators are in the outermost for-loop, so the output tuples\n\
cycle in a manner similar to an odometer (with the rightmost element changing\n\
on every iteration).\n\n\
To compute the product of an iterable with itself, specify the number\n\
of repetitions with the optional repeat keyword argument. For example,\n\
product(A, repeat=4) means the same as product(A, A, A, A).\n\n\
product('ab', range(3)) --> ('a',0) ('a',1) ('a',2) ('b',0) ('b',1) ('b',2)\n\
product((0,1), (0,1), (0,1)) --> (0,0,0) (0,0,1) (0,1,0) (0,1,1) (1,0,0) ...");

static PyTypeObject product_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.product",                /* tp_name */
    sizeof(productobject),              /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)product_dealloc,        /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    product_doc,                        /* tp_doc */
    (traverseproc)product_traverse,     /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)product_next,         /* tp_iternext */
    product_methods,                    /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    product_new,                        /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* combinations object *******************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *pool;         /* input converted to a tuple */
    Py_ssize_t *indices;    /* one index per result element */
    PyObject *result;       /* most recently returned result tuple */
    Py_ssize_t r;           /* size of result tuple */
    int stopped;            /* set to 1 when the iterator is exhausted */
} combinationsobject;

static PyTypeObject combinations_type;

static PyObject *
combinations_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    combinationsobject *co;
    Py_ssize_t n;
    Py_ssize_t r;
    PyObject *pool = NULL;
    PyObject *iterable = NULL;
    Py_ssize_t *indices = NULL;
    Py_ssize_t i;
    static char *kwargs[] = {"iterable", "r", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "On:combinations", kwargs,
                                     &iterable, &r))
        return NULL;

    pool = PySequence_Tuple(iterable);
    if (pool == NULL)
        goto error;
    n = PyTuple_GET_SIZE(pool);
    if (r < 0) {
        PyErr_SetString(PyExc_ValueError, "r must be non-negative");
        goto error;
    }

    indices = PyMem_New(Py_ssize_t, r);
    if (indices == NULL) {
        PyErr_NoMemory();
        goto error;
    }

    for (i=0 ; i<r ; i++)
        indices[i] = i;

    /* create combinationsobject structure */
    co = (combinationsobject *)type->tp_alloc(type, 0);
    if (co == NULL)
        goto error;

    co->pool = pool;
    co->indices = indices;
    co->result = NULL;
    co->r = r;
    co->stopped = r > n ? 1 : 0;

    return (PyObject *)co;

error:
    if (indices != NULL)
        PyMem_Free(indices);
    Py_XDECREF(pool);
    return NULL;
}

static void
combinations_dealloc(combinationsobject *co)
{
    PyObject_GC_UnTrack(co);
    Py_XDECREF(co->pool);
    Py_XDECREF(co->result);
    if (co->indices != NULL)
        PyMem_Free(co->indices);
    Py_TYPE(co)->tp_free(co);
}

static PyObject *
combinations_sizeof(combinationsobject *co, void *unused)
{
    Py_ssize_t res;

    res = _PyObject_SIZE(Py_TYPE(co));
    res += co->r * sizeof(Py_ssize_t);
    return PyLong_FromSsize_t(res);
}

static int
combinations_traverse(combinationsobject *co, visitproc visit, void *arg)
{
    Py_VISIT(co->pool);
    Py_VISIT(co->result);
    return 0;
}

static PyObject *
combinations_next(combinationsobject *co)
{
    PyObject *elem;
    PyObject *oldelem;
    PyObject *pool = co->pool;
    Py_ssize_t *indices = co->indices;
    PyObject *result = co->result;
    Py_ssize_t n = PyTuple_GET_SIZE(pool);
    Py_ssize_t r = co->r;
    Py_ssize_t i, j, index;

    if (co->stopped)
        return NULL;

    if (result == NULL) {
        /* On the first pass, initialize result tuple using the indices */
        result = PyTuple_New(r);
        if (result == NULL)
            goto empty;
        co->result = result;
        for (i=0; i<r ; i++) {
            index = indices[i];
            elem = PyTuple_GET_ITEM(pool, index);
            Py_INCREF(elem);
            PyTuple_SET_ITEM(result, i, elem);
        }
    } else {
        /* Copy the previous result tuple or re-use it if available */
        if (Py_REFCNT(result) > 1) {
            PyObject *old_result = result;
            result = PyTuple_New(r);
            if (result == NULL)
                goto empty;
            co->result = result;
            for (i=0; i<r ; i++) {
                elem = PyTuple_GET_ITEM(old_result, i);
                Py_INCREF(elem);
                PyTuple_SET_ITEM(result, i, elem);
            }
            Py_DECREF(old_result);
        }
        /* Now, we've got the only copy so we can update it in-place
         * CPython's empty tuple is a singleton and cached in
         * PyTuple's freelist.
         */
        assert(r == 0 || Py_REFCNT(result) == 1);

        /* Scan indices right-to-left until finding one that is not
           at its maximum (i + n - r). */
        for (i=r-1 ; i >= 0 && indices[i] == i+n-r ; i--)
            ;

        /* If i is negative, then the indices are all at
           their maximum value and we're done. */
        if (i < 0)
            goto empty;

        /* Increment the current index which we know is not at its
           maximum.  Then move back to the right setting each index
           to its lowest possible value (one higher than the index
           to its left -- this maintains the sort order invariant). */
        indices[i]++;
        for (j=i+1 ; j<r ; j++)
            indices[j] = indices[j-1] + 1;

        /* Update the result tuple for the new indices
           starting with i, the leftmost index that changed */
        for ( ; i<r ; i++) {
            index = indices[i];
            elem = PyTuple_GET_ITEM(pool, index);
            Py_INCREF(elem);
            oldelem = PyTuple_GET_ITEM(result, i);
            PyTuple_SET_ITEM(result, i, elem);
            Py_DECREF(oldelem);
        }
    }

    Py_INCREF(result);
    return result;

empty:
    co->stopped = 1;
    return NULL;
}

static PyObject *
combinations_reduce(combinationsobject *lz)
{
    if (lz->result == NULL) {
        return Py_BuildValue("O(On)", Py_TYPE(lz), lz->pool, lz->r);
    } else if (lz->stopped) {
        return Py_BuildValue("O(()n)", Py_TYPE(lz), lz->r);
    } else {
        PyObject *indices;
        Py_ssize_t i;

        /* we must pickle the indices and use them for setstate */
        indices = PyTuple_New(lz->r);
        if (!indices)
            return NULL;
        for (i=0; i<lz->r; i++)
        {
            PyObject* index = PyLong_FromSsize_t(lz->indices[i]);
            if (!index) {
                Py_DECREF(indices);
                return NULL;
            }
            PyTuple_SET_ITEM(indices, i, index);
        }

        return Py_BuildValue("O(On)N", Py_TYPE(lz), lz->pool, lz->r, indices);
    }
}

static PyObject *
combinations_setstate(combinationsobject *lz, PyObject *state)
{
    PyObject *result;
    Py_ssize_t i;
    Py_ssize_t n = PyTuple_GET_SIZE(lz->pool);

    if (!PyTuple_Check(state) || PyTuple_GET_SIZE(state) != lz->r) {
        PyErr_SetString(PyExc_ValueError, "invalid arguments");
        return NULL;
    }

    for (i=0; i<lz->r; i++) {
        Py_ssize_t max;
        PyObject* indexObject = PyTuple_GET_ITEM(state, i);
        Py_ssize_t index = PyLong_AsSsize_t(indexObject);

        if (index == -1 && PyErr_Occurred())
            return NULL; /* not an integer */
        max = i + n - lz->r;
        /* clamp the index (beware of negative max) */
        if (index > max)
            index = max;
        if (index < 0)
            index = 0;
        lz->indices[i] = index;
    }

    result = PyTuple_New(lz->r);
    if (result == NULL)
        return NULL;
    for (i=0; i<lz->r; i++) {
        PyObject *element = PyTuple_GET_ITEM(lz->pool, lz->indices[i]);
        Py_INCREF(element);
        PyTuple_SET_ITEM(result, i, element);
    }

    Py_XSETREF(lz->result, result);
    Py_RETURN_NONE;
}

static PyMethodDef combinations_methods[] = {
    {"__reduce__",      (PyCFunction)combinations_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)combinations_setstate,    METH_O,
     setstate_doc},
    {"__sizeof__",      (PyCFunction)combinations_sizeof,      METH_NOARGS,
     sizeof_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(combinations_doc,
"combinations(iterable, r) --> combinations object\n\
\n\
Return successive r-length combinations of elements in the iterable.\n\n\
combinations(range(4), 3) --> (0,1,2), (0,1,3), (0,2,3), (1,2,3)");

static PyTypeObject combinations_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.combinations",           /* tp_name */
    sizeof(combinationsobject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)combinations_dealloc,   /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    combinations_doc,                   /* tp_doc */
    (traverseproc)combinations_traverse,/* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)combinations_next,    /* tp_iternext */
    combinations_methods,               /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    combinations_new,                   /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* combinations with replacement object **************************************/

/* Equivalent to:

        def combinations_with_replacement(iterable, r):
            "combinations_with_replacement('ABC', 2) --> AA AB AC BB BC CC"
            # number items returned:  (n+r-1)! / r! / (n-1)!
            pool = tuple(iterable)
            n = len(pool)
            indices = [0] * r
            yield tuple(pool[i] for i in indices)
            while 1:
                for i in reversed(range(r)):
                    if indices[i] != n - 1:
                        break
                else:
                    return
                indices[i:] = [indices[i] + 1] * (r - i)
                yield tuple(pool[i] for i in indices)

        def combinations_with_replacement2(iterable, r):
            'Alternate version that filters from product()'
            pool = tuple(iterable)
            n = len(pool)
            for indices in product(range(n), repeat=r):
                if sorted(indices) == list(indices):
                    yield tuple(pool[i] for i in indices)
*/
typedef struct {
    PyObject_HEAD
    PyObject *pool;         /* input converted to a tuple */
    Py_ssize_t *indices;    /* one index per result element */
    PyObject *result;       /* most recently returned result tuple */
    Py_ssize_t r;           /* size of result tuple */
    int stopped;            /* set to 1 when the cwr iterator is exhausted */
} cwrobject;

static PyTypeObject cwr_type;

static PyObject *
cwr_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    cwrobject *co;
    Py_ssize_t n;
    Py_ssize_t r;
    PyObject *pool = NULL;
    PyObject *iterable = NULL;
    Py_ssize_t *indices = NULL;
    Py_ssize_t i;
    static char *kwargs[] = {"iterable", "r", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds,
                                     "On:combinations_with_replacement",
                                     kwargs, &iterable, &r))
        return NULL;

    pool = PySequence_Tuple(iterable);
    if (pool == NULL)
        goto error;
    n = PyTuple_GET_SIZE(pool);
    if (r < 0) {
        PyErr_SetString(PyExc_ValueError, "r must be non-negative");
        goto error;
    }

    indices = PyMem_New(Py_ssize_t, r);
    if (indices == NULL) {
        PyErr_NoMemory();
        goto error;
    }

    for (i=0 ; i<r ; i++)
        indices[i] = 0;

    /* create cwrobject structure */
    co = (cwrobject *)type->tp_alloc(type, 0);
    if (co == NULL)
        goto error;

    co->pool = pool;
    co->indices = indices;
    co->result = NULL;
    co->r = r;
    co->stopped = !n && r;

    return (PyObject *)co;

error:
    if (indices != NULL)
        PyMem_Free(indices);
    Py_XDECREF(pool);
    return NULL;
}

static void
cwr_dealloc(cwrobject *co)
{
    PyObject_GC_UnTrack(co);
    Py_XDECREF(co->pool);
    Py_XDECREF(co->result);
    if (co->indices != NULL)
        PyMem_Free(co->indices);
    Py_TYPE(co)->tp_free(co);
}

static PyObject *
cwr_sizeof(cwrobject *co, void *unused)
{
    Py_ssize_t res;

    res = _PyObject_SIZE(Py_TYPE(co));
    res += co->r * sizeof(Py_ssize_t);
    return PyLong_FromSsize_t(res);
}

static int
cwr_traverse(cwrobject *co, visitproc visit, void *arg)
{
    Py_VISIT(co->pool);
    Py_VISIT(co->result);
    return 0;
}

static PyObject *
cwr_next(cwrobject *co)
{
    PyObject *elem;
    PyObject *oldelem;
    PyObject *pool = co->pool;
    Py_ssize_t *indices = co->indices;
    PyObject *result = co->result;
    Py_ssize_t n = PyTuple_GET_SIZE(pool);
    Py_ssize_t r = co->r;
    Py_ssize_t i, index;

    if (co->stopped)
        return NULL;

    if (result == NULL) {
        /* On the first pass, initialize result tuple with pool[0] */
        result = PyTuple_New(r);
        if (result == NULL)
            goto empty;
        co->result = result;
        if (n > 0) {
            elem = PyTuple_GET_ITEM(pool, 0);
            for (i=0; i<r ; i++) {
                assert(indices[i] == 0);
                Py_INCREF(elem);
                PyTuple_SET_ITEM(result, i, elem);
            }
        }
    } else {
        /* Copy the previous result tuple or re-use it if available */
        if (Py_REFCNT(result) > 1) {
            PyObject *old_result = result;
            result = PyTuple_New(r);
            if (result == NULL)
                goto empty;
            co->result = result;
            for (i=0; i<r ; i++) {
                elem = PyTuple_GET_ITEM(old_result, i);
                Py_INCREF(elem);
                PyTuple_SET_ITEM(result, i, elem);
            }
            Py_DECREF(old_result);
        }
        /* Now, we've got the only copy so we can update it in-place CPython's
           empty tuple is a singleton and cached in PyTuple's freelist. */
        assert(r == 0 || Py_REFCNT(result) == 1);

       /* Scan indices right-to-left until finding one that is not
        * at its maximum (n-1). */
        for (i=r-1 ; i >= 0 && indices[i] == n-1; i--)
            ;

        /* If i is negative, then the indices are all at
           their maximum value and we're done. */
        if (i < 0)
            goto empty;

        /* Increment the current index which we know is not at its
           maximum.  Then set all to the right to the same value. */
        index = indices[i] + 1;
        assert(index < n);
        elem = PyTuple_GET_ITEM(pool, index);
        for ( ; i<r ; i++) {
            indices[i] = index;
            Py_INCREF(elem);
            oldelem = PyTuple_GET_ITEM(result, i);
            PyTuple_SET_ITEM(result, i, elem);
            Py_DECREF(oldelem);
        }
    }

    Py_INCREF(result);
    return result;

empty:
    co->stopped = 1;
    return NULL;
}

static PyObject *
cwr_reduce(cwrobject *lz)
{
    if (lz->result == NULL) {
        return Py_BuildValue("O(On)", Py_TYPE(lz), lz->pool, lz->r);
    } else if (lz->stopped) {
        return Py_BuildValue("O(()n)", Py_TYPE(lz), lz->r);
    } else {
        PyObject *indices;
        Py_ssize_t i;

        /* we must pickle the indices and use them for setstate */
        indices = PyTuple_New(lz->r);
        if (!indices)
            return NULL;
        for (i=0; i<lz->r; i++) {
            PyObject* index = PyLong_FromSsize_t(lz->indices[i]);
            if (!index) {
                Py_DECREF(indices);
                return NULL;
            }
            PyTuple_SET_ITEM(indices, i, index);
        }

        return Py_BuildValue("O(On)N", Py_TYPE(lz), lz->pool, lz->r, indices);
    }
}

static PyObject *
cwr_setstate(cwrobject *lz, PyObject *state)
{
    PyObject *result;
    Py_ssize_t n, i;

    if (!PyTuple_Check(state) || PyTuple_GET_SIZE(state) != lz->r)
    {
        PyErr_SetString(PyExc_ValueError, "invalid arguments");
        return NULL;
    }

    n = PyTuple_GET_SIZE(lz->pool);
    for (i=0; i<lz->r; i++) {
        PyObject* indexObject = PyTuple_GET_ITEM(state, i);
        Py_ssize_t index = PyLong_AsSsize_t(indexObject);

        if (index < 0 && PyErr_Occurred())
            return NULL; /* not an integer */
        /* clamp the index */
        if (index < 0)
            index = 0;
        else if (index > n-1)
            index = n-1;
        lz->indices[i] = index;
    }
    result = PyTuple_New(lz->r);
    if (result == NULL)
        return NULL;
    for (i=0; i<lz->r; i++) {
        PyObject *element = PyTuple_GET_ITEM(lz->pool, lz->indices[i]);
        Py_INCREF(element);
        PyTuple_SET_ITEM(result, i, element);
    }
    Py_XSETREF(lz->result, result);
    Py_RETURN_NONE;
}

static PyMethodDef cwr_methods[] = {
    {"__reduce__",      (PyCFunction)cwr_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)cwr_setstate,    METH_O,
     setstate_doc},
    {"__sizeof__",      (PyCFunction)cwr_sizeof,      METH_NOARGS,
     sizeof_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(cwr_doc,
"combinations_with_replacement(iterable, r) --> combinations_with_replacement object\n\
\n\
Return successive r-length combinations of elements in the iterable\n\
allowing individual elements to have successive repeats.\n\
combinations_with_replacement('ABC', 2) --> AA AB AC BB BC CC");

static PyTypeObject cwr_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.combinations_with_replacement",          /* tp_name */
    sizeof(cwrobject),                                  /* tp_basicsize */
    0,                                                  /* tp_itemsize */
    /* methods */
    (destructor)cwr_dealloc,                            /* tp_dealloc */
    0,                                                  /* tp_print */
    0,                                                  /* tp_getattr */
    0,                                                  /* tp_setattr */
    0,                                                  /* tp_reserved */
    0,                                                  /* tp_repr */
    0,                                                  /* tp_as_number */
    0,                                                  /* tp_as_sequence */
    0,                                                  /* tp_as_mapping */
    0,                                                  /* tp_hash */
    0,                                                  /* tp_call */
    0,                                                  /* tp_str */
    PyObject_GenericGetAttr,                            /* tp_getattro */
    0,                                                  /* tp_setattro */
    0,                                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,                            /* tp_flags */
    cwr_doc,                                            /* tp_doc */
    (traverseproc)cwr_traverse,                         /* tp_traverse */
    0,                                                  /* tp_clear */
    0,                                                  /* tp_richcompare */
    0,                                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                                  /* tp_iter */
    (iternextfunc)cwr_next,                             /* tp_iternext */
    cwr_methods,                                        /* tp_methods */
    0,                                                  /* tp_members */
    0,                                                  /* tp_getset */
    0,                                                  /* tp_base */
    0,                                                  /* tp_dict */
    0,                                                  /* tp_descr_get */
    0,                                                  /* tp_descr_set */
    0,                                                  /* tp_dictoffset */
    0,                                                  /* tp_init */
    0,                                                  /* tp_alloc */
    cwr_new,                                            /* tp_new */
    PyObject_GC_Del,                                    /* tp_free */
};


/* permutations object ********************************************************

def permutations(iterable, r=None):
    'permutations(range(3), 2) --> (0,1) (0,2) (1,0) (1,2) (2,0) (2,1)'
    pool = tuple(iterable)
    n = len(pool)
    r = n if r is None else r
    indices = range(n)
    cycles = range(n-r+1, n+1)[::-1]
    yield tuple(pool[i] for i in indices[:r])
    while n:
        for i in reversed(range(r)):
            cycles[i] -= 1
            if cycles[i] == 0:
                indices[i:] = indices[i+1:] + indices[i:i+1]
                cycles[i] = n - i
            else:
                j = cycles[i]
                indices[i], indices[-j] = indices[-j], indices[i]
                yield tuple(pool[i] for i in indices[:r])
                break
        else:
            return
*/

typedef struct {
    PyObject_HEAD
    PyObject *pool;         /* input converted to a tuple */
    Py_ssize_t *indices;    /* one index per element in the pool */
    Py_ssize_t *cycles;     /* one rollover counter per element in the result */
    PyObject *result;       /* most recently returned result tuple */
    Py_ssize_t r;           /* size of result tuple */
    int stopped;            /* set to 1 when the iterator is exhausted */
} permutationsobject;

static PyTypeObject permutations_type;

static PyObject *
permutations_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    permutationsobject *po;
    Py_ssize_t n;
    Py_ssize_t r;
    PyObject *robj = Py_None;
    PyObject *pool = NULL;
    PyObject *iterable = NULL;
    Py_ssize_t *indices = NULL;
    Py_ssize_t *cycles = NULL;
    Py_ssize_t i;
    static char *kwargs[] = {"iterable", "r", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:permutations", kwargs,
                                     &iterable, &robj))
        return NULL;

    pool = PySequence_Tuple(iterable);
    if (pool == NULL)
        goto error;
    n = PyTuple_GET_SIZE(pool);

    r = n;
    if (robj != Py_None) {
        if (!PyLong_Check(robj)) {
            PyErr_SetString(PyExc_TypeError, "Expected int as r");
            goto error;
        }
        r = PyLong_AsSsize_t(robj);
        if (r == -1 && PyErr_Occurred())
            goto error;
    }
    if (r < 0) {
        PyErr_SetString(PyExc_ValueError, "r must be non-negative");
        goto error;
    }

    indices = PyMem_New(Py_ssize_t, n);
    cycles = PyMem_New(Py_ssize_t, r);
    if (indices == NULL || cycles == NULL) {
        PyErr_NoMemory();
        goto error;
    }

    for (i=0 ; i<n ; i++)
        indices[i] = i;
    for (i=0 ; i<r ; i++)
        cycles[i] = n - i;

    /* create permutationsobject structure */
    po = (permutationsobject *)type->tp_alloc(type, 0);
    if (po == NULL)
        goto error;

    po->pool = pool;
    po->indices = indices;
    po->cycles = cycles;
    po->result = NULL;
    po->r = r;
    po->stopped = r > n ? 1 : 0;

    return (PyObject *)po;

error:
    if (indices != NULL)
        PyMem_Free(indices);
    if (cycles != NULL)
        PyMem_Free(cycles);
    Py_XDECREF(pool);
    return NULL;
}

static void
permutations_dealloc(permutationsobject *po)
{
    PyObject_GC_UnTrack(po);
    Py_XDECREF(po->pool);
    Py_XDECREF(po->result);
    PyMem_Free(po->indices);
    PyMem_Free(po->cycles);
    Py_TYPE(po)->tp_free(po);
}

static PyObject *
permutations_sizeof(permutationsobject *po, void *unused)
{
    Py_ssize_t res;

    res = _PyObject_SIZE(Py_TYPE(po));
    res += PyTuple_GET_SIZE(po->pool) * sizeof(Py_ssize_t);
    res += po->r * sizeof(Py_ssize_t);
    return PyLong_FromSsize_t(res);
}

static int
permutations_traverse(permutationsobject *po, visitproc visit, void *arg)
{
    Py_VISIT(po->pool);
    Py_VISIT(po->result);
    return 0;
}

static PyObject *
permutations_next(permutationsobject *po)
{
    PyObject *elem;
    PyObject *oldelem;
    PyObject *pool = po->pool;
    Py_ssize_t *indices = po->indices;
    Py_ssize_t *cycles = po->cycles;
    PyObject *result = po->result;
    Py_ssize_t n = PyTuple_GET_SIZE(pool);
    Py_ssize_t r = po->r;
    Py_ssize_t i, j, k, index;

    if (po->stopped)
        return NULL;

    if (result == NULL) {
        /* On the first pass, initialize result tuple using the indices */
        result = PyTuple_New(r);
        if (result == NULL)
            goto empty;
        po->result = result;
        for (i=0; i<r ; i++) {
            index = indices[i];
            elem = PyTuple_GET_ITEM(pool, index);
            Py_INCREF(elem);
            PyTuple_SET_ITEM(result, i, elem);
        }
    } else {
        if (n == 0)
            goto empty;

        /* Copy the previous result tuple or re-use it if available */
        if (Py_REFCNT(result) > 1) {
            PyObject *old_result = result;
            result = PyTuple_New(r);
            if (result == NULL)
                goto empty;
            po->result = result;
            for (i=0; i<r ; i++) {
                elem = PyTuple_GET_ITEM(old_result, i);
                Py_INCREF(elem);
                PyTuple_SET_ITEM(result, i, elem);
            }
            Py_DECREF(old_result);
        }
        /* Now, we've got the only copy so we can update it in-place */
        assert(r == 0 || Py_REFCNT(result) == 1);

        /* Decrement rightmost cycle, moving leftward upon zero rollover */
        for (i=r-1 ; i>=0 ; i--) {
            cycles[i] -= 1;
            if (cycles[i] == 0) {
                /* rotatation: indices[i:] = indices[i+1:] + indices[i:i+1] */
                index = indices[i];
                for (j=i ; j<n-1 ; j++)
                    indices[j] = indices[j+1];
                indices[n-1] = index;
                cycles[i] = n - i;
            } else {
                j = cycles[i];
                index = indices[i];
                indices[i] = indices[n-j];
                indices[n-j] = index;

                for (k=i; k<r ; k++) {
                    /* start with i, the leftmost element that changed */
                    /* yield tuple(pool[k] for k in indices[:r]) */
                    index = indices[k];
                    elem = PyTuple_GET_ITEM(pool, index);
                    Py_INCREF(elem);
                    oldelem = PyTuple_GET_ITEM(result, k);
                    PyTuple_SET_ITEM(result, k, elem);
                    Py_DECREF(oldelem);
                }
                break;
            }
        }
        /* If i is negative, then the cycles have all
           rolled-over and we're done. */
        if (i < 0)
            goto empty;
    }
    Py_INCREF(result);
    return result;

empty:
    po->stopped = 1;
    return NULL;
}

static PyObject *
permutations_reduce(permutationsobject *po)
{
    if (po->result == NULL) {
        return Py_BuildValue("O(On)", Py_TYPE(po), po->pool, po->r);
    } else if (po->stopped) {
        return Py_BuildValue("O(()n)", Py_TYPE(po), po->r);
    } else {
        PyObject *indices=NULL, *cycles=NULL;
        Py_ssize_t n, i;

        /* we must pickle the indices and cycles and use them for setstate */
        n = PyTuple_GET_SIZE(po->pool);
        indices = PyTuple_New(n);
        if (indices == NULL)
            goto err;
        for (i=0; i<n; i++) {
            PyObject* index = PyLong_FromSsize_t(po->indices[i]);
            if (!index)
                goto err;
            PyTuple_SET_ITEM(indices, i, index);
        }

        cycles = PyTuple_New(po->r);
        if (cycles == NULL)
            goto err;
        for (i=0 ; i<po->r ; i++) {
            PyObject* index = PyLong_FromSsize_t(po->cycles[i]);
            if (!index)
                goto err;
            PyTuple_SET_ITEM(cycles, i, index);
        }
        return Py_BuildValue("O(On)(NN)", Py_TYPE(po),
                             po->pool, po->r,
                             indices, cycles);
    err:
        Py_XDECREF(indices);
        Py_XDECREF(cycles);
        return NULL;
    }
}

static PyObject *
permutations_setstate(permutationsobject *po, PyObject *state)
{
    PyObject *indices, *cycles, *result;
    Py_ssize_t n, i;

    if (!PyTuple_Check(state)) {
        PyErr_SetString(PyExc_TypeError, "state is not a tuple");
        return NULL;
    }
    if (!PyArg_ParseTuple(state, "O!O!",
                          &PyTuple_Type, &indices,
                          &PyTuple_Type, &cycles)) {
        return NULL;
    }

    n = PyTuple_GET_SIZE(po->pool);
    if (PyTuple_GET_SIZE(indices) != n || PyTuple_GET_SIZE(cycles) != po->r) {
        PyErr_SetString(PyExc_ValueError, "invalid arguments");
        return NULL;
    }

    for (i=0; i<n; i++) {
        PyObject* indexObject = PyTuple_GET_ITEM(indices, i);
        Py_ssize_t index = PyLong_AsSsize_t(indexObject);
        if (index < 0 && PyErr_Occurred())
            return NULL; /* not an integer */
        /* clamp the index */
        if (index < 0)
            index = 0;
        else if (index > n-1)
            index = n-1;
        po->indices[i] = index;
    }

    for (i=0; i<po->r; i++) {
        PyObject* indexObject = PyTuple_GET_ITEM(cycles, i);
        Py_ssize_t index = PyLong_AsSsize_t(indexObject);
        if (index < 0 && PyErr_Occurred())
            return NULL; /* not an integer */
        if (index < 1)
            index = 1;
        else if (index > n-i)
            index = n-i;
        po->cycles[i] = index;
    }
    result = PyTuple_New(po->r);
    if (result == NULL)
        return NULL;
    for (i=0; i<po->r; i++) {
        PyObject *element = PyTuple_GET_ITEM(po->pool, po->indices[i]);
        Py_INCREF(element);
        PyTuple_SET_ITEM(result, i, element);
    }
    Py_XSETREF(po->result, result);
    Py_RETURN_NONE;
}

static PyMethodDef permuations_methods[] = {
    {"__reduce__",      (PyCFunction)permutations_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)permutations_setstate,    METH_O,
     setstate_doc},
    {"__sizeof__",      (PyCFunction)permutations_sizeof,      METH_NOARGS,
     sizeof_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(permutations_doc,
"permutations(iterable[, r]) --> permutations object\n\
\n\
Return successive r-length permutations of elements in the iterable.\n\n\
permutations(range(3), 2) --> (0,1), (0,2), (1,0), (1,2), (2,0), (2,1)");

static PyTypeObject permutations_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.permutations",           /* tp_name */
    sizeof(permutationsobject),         /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)permutations_dealloc,   /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    permutations_doc,                   /* tp_doc */
    (traverseproc)permutations_traverse,/* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)permutations_next,    /* tp_iternext */
    permuations_methods,                /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    permutations_new,                   /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};

/* accumulate object ********************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *total;
    PyObject *it;
    PyObject *binop;
} accumulateobject;

static PyTypeObject accumulate_type;

static PyObject *
accumulate_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    static char *kwargs[] = {"iterable", "func", NULL};
    PyObject *iterable;
    PyObject *it;
    PyObject *binop = Py_None;
    accumulateobject *lz;

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O:accumulate",
                                     kwargs, &iterable, &binop))
        return NULL;

    /* Get iterator. */
    it = PyObject_GetIter(iterable);
    if (it == NULL)
        return NULL;

    /* create accumulateobject structure */
    lz = (accumulateobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(it);
        return NULL;
    }

    if (binop != Py_None) {
        Py_XINCREF(binop);
        lz->binop = binop;
    }
    lz->total = NULL;
    lz->it = it;
    return (PyObject *)lz;
}

static void
accumulate_dealloc(accumulateobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->binop);
    Py_XDECREF(lz->total);
    Py_XDECREF(lz->it);
    Py_TYPE(lz)->tp_free(lz);
}

static int
accumulate_traverse(accumulateobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->binop);
    Py_VISIT(lz->it);
    Py_VISIT(lz->total);
    return 0;
}

static PyObject *
accumulate_next(accumulateobject *lz)
{
    PyObject *val, *newtotal;

    val = (*Py_TYPE(lz->it)->tp_iternext)(lz->it);
    if (val == NULL)
        return NULL;

    if (lz->total == NULL) {
        Py_INCREF(val);
        lz->total = val;
        return lz->total;
    }

    if (lz->binop == NULL)
        newtotal = PyNumber_Add(lz->total, val);
    else
        newtotal = PyObject_CallFunctionObjArgs(lz->binop, lz->total, val, NULL);
    Py_DECREF(val);
    if (newtotal == NULL)
        return NULL;

    Py_INCREF(newtotal);
    Py_SETREF(lz->total, newtotal);
    return newtotal;
}

static PyObject *
accumulate_reduce(accumulateobject *lz)
{
    if (lz->total == Py_None) {
        PyObject *it;

        if (PyType_Ready(&chain_type) < 0)
            return NULL;
        if (PyType_Ready(&islice_type) < 0)
            return NULL;
        it = PyObject_CallFunction((PyObject *)&chain_type, "(O)O",
                                   lz->total, lz->it);
        if (it == NULL)
            return NULL;
        it = PyObject_CallFunction((PyObject *)Py_TYPE(lz), "NO",
                                   it, lz->binop ? lz->binop : Py_None);
        if (it == NULL)
            return NULL;
        return Py_BuildValue("O(NiO)", &islice_type, it, 1, Py_None);
    }
    return Py_BuildValue("O(OO)O", Py_TYPE(lz),
                            lz->it, lz->binop?lz->binop:Py_None,
                            lz->total?lz->total:Py_None);
}

static PyObject *
accumulate_setstate(accumulateobject *lz, PyObject *state)
{
    Py_INCREF(state);
    Py_XSETREF(lz->total, state);
    Py_RETURN_NONE;
}

static PyMethodDef accumulate_methods[] = {
    {"__reduce__",      (PyCFunction)accumulate_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)accumulate_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(accumulate_doc,
"accumulate(iterable[, func]) --> accumulate object\n\
\n\
Return series of accumulated sums (or other binary function results).");

static PyTypeObject accumulate_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.accumulate",             /* tp_name */
    sizeof(accumulateobject),           /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)accumulate_dealloc,     /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    accumulate_doc,                     /* tp_doc */
    (traverseproc)accumulate_traverse,  /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)accumulate_next,      /* tp_iternext */
    accumulate_methods,                 /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    accumulate_new,                     /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* compress object ************************************************************/

/* Equivalent to:

    def compress(data, selectors):
        "compress('ABCDEF', [1,0,1,0,1,1]) --> A C E F"
        return (d for d, s in zip(data, selectors) if s)
*/

typedef struct {
    PyObject_HEAD
    PyObject *data;
    PyObject *selectors;
} compressobject;

static PyTypeObject compress_type;

static PyObject *
compress_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *seq1, *seq2;
    PyObject *data=NULL, *selectors=NULL;
    compressobject *lz;
    static char *kwargs[] = {"data", "selectors", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO:compress", kwargs, &seq1, &seq2))
        return NULL;

    data = PyObject_GetIter(seq1);
    if (data == NULL)
        goto fail;
    selectors = PyObject_GetIter(seq2);
    if (selectors == NULL)
        goto fail;

    /* create compressobject structure */
    lz = (compressobject *)type->tp_alloc(type, 0);
    if (lz == NULL)
        goto fail;
    lz->data = data;
    lz->selectors = selectors;
    return (PyObject *)lz;

fail:
    Py_XDECREF(data);
    Py_XDECREF(selectors);
    return NULL;
}

static void
compress_dealloc(compressobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->data);
    Py_XDECREF(lz->selectors);
    Py_TYPE(lz)->tp_free(lz);
}

static int
compress_traverse(compressobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->data);
    Py_VISIT(lz->selectors);
    return 0;
}

static PyObject *
compress_next(compressobject *lz)
{
    PyObject *data = lz->data, *selectors = lz->selectors;
    PyObject *datum, *selector;
    PyObject *(*datanext)(PyObject *) = *Py_TYPE(data)->tp_iternext;
    PyObject *(*selectornext)(PyObject *) = *Py_TYPE(selectors)->tp_iternext;
    int ok;

    while (1) {
        /* Steps:  get datum, get selector, evaluate selector.
           Order is important (to match the pure python version
           in terms of which input gets a chance to raise an
           exception first).
        */

        datum = datanext(data);
        if (datum == NULL)
            return NULL;

        selector = selectornext(selectors);
        if (selector == NULL) {
            Py_DECREF(datum);
            return NULL;
        }

        ok = PyObject_IsTrue(selector);
        Py_DECREF(selector);
        if (ok > 0)
            return datum;
        Py_DECREF(datum);
        if (ok < 0)
            return NULL;
    }
}

static PyObject *
compress_reduce(compressobject *lz)
{
    return Py_BuildValue("O(OO)", Py_TYPE(lz),
        lz->data, lz->selectors);
}

static PyMethodDef compress_methods[] = {
    {"__reduce__",      (PyCFunction)compress_reduce,      METH_NOARGS,
     reduce_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(compress_doc,
"compress(data, selectors) --> iterator over selected data\n\
\n\
Return data elements corresponding to true selector elements.\n\
Forms a shorter iterator from selected data elements using the\n\
selectors to choose the data elements.");

static PyTypeObject compress_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.compress",               /* tp_name */
    sizeof(compressobject),             /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)compress_dealloc,       /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    compress_doc,                       /* tp_doc */
    (traverseproc)compress_traverse,    /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)compress_next,        /* tp_iternext */
    compress_methods,                   /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    compress_new,                       /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* filterfalse object ************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *func;
    PyObject *it;
} filterfalseobject;

static PyTypeObject filterfalse_type;

static PyObject *
filterfalse_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    PyObject *func, *seq;
    PyObject *it;
    filterfalseobject *lz;

    if (type == &filterfalse_type &&
        !_PyArg_NoKeywords("filterfalse()", kwds))
        return NULL;

    if (!PyArg_UnpackTuple(args, "filterfalse", 2, 2, &func, &seq))
        return NULL;

    /* Get iterator. */
    it = PyObject_GetIter(seq);
    if (it == NULL)
        return NULL;

    /* create filterfalseobject structure */
    lz = (filterfalseobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(it);
        return NULL;
    }
    Py_INCREF(func);
    lz->func = func;
    lz->it = it;

    return (PyObject *)lz;
}

static void
filterfalse_dealloc(filterfalseobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->func);
    Py_XDECREF(lz->it);
    Py_TYPE(lz)->tp_free(lz);
}

static int
filterfalse_traverse(filterfalseobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->it);
    Py_VISIT(lz->func);
    return 0;
}

static PyObject *
filterfalse_next(filterfalseobject *lz)
{
    PyObject *item;
    PyObject *it = lz->it;
    long ok;
    PyObject *(*iternext)(PyObject *);

    iternext = *Py_TYPE(it)->tp_iternext;
    for (;;) {
        item = iternext(it);
        if (item == NULL)
            return NULL;

        if (lz->func == Py_None || lz->func == (PyObject *)&PyBool_Type) {
            ok = PyObject_IsTrue(item);
        } else {
            PyObject *good;
            good = PyObject_CallFunctionObjArgs(lz->func, item, NULL);
            if (good == NULL) {
                Py_DECREF(item);
                return NULL;
            }
            ok = PyObject_IsTrue(good);
            Py_DECREF(good);
        }
        if (ok == 0)
            return item;
        Py_DECREF(item);
        if (ok < 0)
            return NULL;
    }
}

static PyObject *
filterfalse_reduce(filterfalseobject *lz)
{
    return Py_BuildValue("O(OO)", Py_TYPE(lz), lz->func, lz->it);
}

static PyMethodDef filterfalse_methods[] = {
    {"__reduce__",      (PyCFunction)filterfalse_reduce,      METH_NOARGS,
     reduce_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(filterfalse_doc,
"filterfalse(function or None, sequence) --> filterfalse object\n\
\n\
Return those items of sequence for which function(item) is false.\n\
If function is None, return the items that are false.");

static PyTypeObject filterfalse_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.filterfalse",            /* tp_name */
    sizeof(filterfalseobject),          /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)filterfalse_dealloc,    /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    filterfalse_doc,                    /* tp_doc */
    (traverseproc)filterfalse_traverse, /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)filterfalse_next,     /* tp_iternext */
    filterfalse_methods,                /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    filterfalse_new,                    /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* count object ************************************************************/

typedef struct {
    PyObject_HEAD
    Py_ssize_t cnt;
    PyObject *long_cnt;
    PyObject *long_step;
} countobject;

/* Counting logic and invariants:

fast_mode:  when cnt an integer < PY_SSIZE_T_MAX and no step is specified.

    assert(cnt != PY_SSIZE_T_MAX && long_cnt == NULL && long_step==PyLong(1));
    Advances with:  cnt += 1
    When count hits Y_SSIZE_T_MAX, switch to slow_mode.

slow_mode:  when cnt == PY_SSIZE_T_MAX, step is not int(1), or cnt is a float.

    assert(cnt == PY_SSIZE_T_MAX && long_cnt != NULL && long_step != NULL);
    All counting is done with python objects (no overflows or underflows).
    Advances with:  long_cnt += long_step
    Step may be zero -- effectively a slow version of repeat(cnt).
    Either long_cnt or long_step may be a float, Fraction, or Decimal.
*/

static PyTypeObject count_type;

static PyObject *
count_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    countobject *lz;
    int fast_mode;
    Py_ssize_t cnt = 0;
    PyObject *long_cnt = NULL;
    PyObject *long_step = NULL;
    long step;
    static char *kwlist[] = {"start", "step", 0};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|OO:count",
                    kwlist, &long_cnt, &long_step))
        return NULL;

    if ((long_cnt != NULL && !PyNumber_Check(long_cnt)) ||
        (long_step != NULL && !PyNumber_Check(long_step))) {
                    PyErr_SetString(PyExc_TypeError, "a number is required");
                    return NULL;
    }

    fast_mode = (long_cnt == NULL || PyLong_Check(long_cnt)) &&
                (long_step == NULL || PyLong_Check(long_step));

    /* If not specified, start defaults to 0 */
    if (long_cnt != NULL) {
        if (fast_mode) {
            assert(PyLong_Check(long_cnt));
            cnt = PyLong_AsSsize_t(long_cnt);
            if (cnt == -1 && PyErr_Occurred()) {
                PyErr_Clear();
                fast_mode = 0;
            }
        }
        Py_INCREF(long_cnt);
    } else {
        cnt = 0;
        long_cnt = PyLong_FromLong(0);
        if (long_cnt == NULL) {
            return NULL;
        }
    }

    /* If not specified, step defaults to 1 */
    if (long_step == NULL) {
        long_step = PyLong_FromLong(1);
        if (long_step == NULL) {
            Py_DECREF(long_cnt);
            return NULL;
        }
    } else
        Py_INCREF(long_step);

    assert(long_cnt != NULL && long_step != NULL);

    /* Fast mode only works when the step is 1 */
    if (fast_mode) {
        assert(PyLong_Check(long_step));
        step = PyLong_AsLong(long_step);
        if (step != 1) {
            fast_mode = 0;
            if (step == -1 && PyErr_Occurred())
                PyErr_Clear();
        }
    }

    if (fast_mode)
        Py_CLEAR(long_cnt);
    else
        cnt = PY_SSIZE_T_MAX;

    assert((cnt != PY_SSIZE_T_MAX && long_cnt == NULL && fast_mode) ||
           (cnt == PY_SSIZE_T_MAX && long_cnt != NULL && !fast_mode));
    assert(!fast_mode ||
           (PyLong_Check(long_step) && PyLong_AS_LONG(long_step) == 1));

    /* create countobject structure */
    lz = (countobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_XDECREF(long_cnt);
        return NULL;
    }
    lz->cnt = cnt;
    lz->long_cnt = long_cnt;
    lz->long_step = long_step;

    return (PyObject *)lz;
}

static void
count_dealloc(countobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->long_cnt);
    Py_XDECREF(lz->long_step);
    Py_TYPE(lz)->tp_free(lz);
}

static int
count_traverse(countobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->long_cnt);
    Py_VISIT(lz->long_step);
    return 0;
}

static PyObject *
count_nextlong(countobject *lz)
{
    PyObject *long_cnt;
    PyObject *stepped_up;

    long_cnt = lz->long_cnt;
    if (long_cnt == NULL) {
        /* Switch to slow_mode */
        long_cnt = PyLong_FromSsize_t(PY_SSIZE_T_MAX);
        if (long_cnt == NULL)
            return NULL;
    }
    assert(lz->cnt == PY_SSIZE_T_MAX && long_cnt != NULL);

    stepped_up = PyNumber_Add(long_cnt, lz->long_step);
    if (stepped_up == NULL)
        return NULL;
    lz->long_cnt = stepped_up;
    return long_cnt;
}

static PyObject *
count_next(countobject *lz)
{
    if (lz->cnt == PY_SSIZE_T_MAX)
        return count_nextlong(lz);
    return PyLong_FromSsize_t(lz->cnt++);
}

static PyObject *
count_repr(countobject *lz)
{
    if (lz->cnt != PY_SSIZE_T_MAX)
        return PyUnicode_FromFormat("count(%zd)", lz->cnt);

    if (PyLong_Check(lz->long_step)) {
        long step = PyLong_AsLong(lz->long_step);
        if (step == -1 && PyErr_Occurred()) {
            PyErr_Clear();
        }
        if (step == 1) {
            /* Don't display step when it is an integer equal to 1 */
            return PyUnicode_FromFormat("count(%R)", lz->long_cnt);
        }
    }
    return PyUnicode_FromFormat("count(%R, %R)",
                                                            lz->long_cnt, lz->long_step);
}

static PyObject *
count_reduce(countobject *lz)
{
    if (lz->cnt == PY_SSIZE_T_MAX)
        return Py_BuildValue("O(OO)", Py_TYPE(lz), lz->long_cnt, lz->long_step);
    return Py_BuildValue("O(n)", Py_TYPE(lz), lz->cnt);
}

static PyMethodDef count_methods[] = {
    {"__reduce__",      (PyCFunction)count_reduce,      METH_NOARGS,
     reduce_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(count_doc,
                         "count(start=0, step=1) --> count object\n\
\n\
Return a count object whose .__next__() method returns consecutive values.\n\
Equivalent to:\n\n\
    def count(firstval=0, step=1):\n\
        x = firstval\n\
        while 1:\n\
            yield x\n\
            x += step\n");

static PyTypeObject count_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.count",                  /* tp_name */
    sizeof(countobject),                /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)count_dealloc,          /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    (reprfunc)count_repr,               /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    count_doc,                          /* tp_doc */
    (traverseproc)count_traverse,       /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)count_next,           /* tp_iternext */
    count_methods,                      /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    count_new,                          /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};


/* repeat object ************************************************************/

typedef struct {
    PyObject_HEAD
    PyObject *element;
    Py_ssize_t cnt;
} repeatobject;

static PyTypeObject repeat_type;

static PyObject *
repeat_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    repeatobject *ro;
    PyObject *element;
    Py_ssize_t cnt = -1, n_kwds = 0;
    static char *kwargs[] = {"object", "times", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|n:repeat", kwargs,
                                     &element, &cnt))
        return NULL;

    if (kwds != NULL)
        n_kwds = PyDict_Size(kwds);
    /* Does user supply times argument? */
    if ((PyTuple_Size(args) + n_kwds == 2) && cnt < 0)
        cnt = 0;

    ro = (repeatobject *)type->tp_alloc(type, 0);
    if (ro == NULL)
        return NULL;
    Py_INCREF(element);
    ro->element = element;
    ro->cnt = cnt;
    return (PyObject *)ro;
}

static void
repeat_dealloc(repeatobject *ro)
{
    PyObject_GC_UnTrack(ro);
    Py_XDECREF(ro->element);
    Py_TYPE(ro)->tp_free(ro);
}

static int
repeat_traverse(repeatobject *ro, visitproc visit, void *arg)
{
    Py_VISIT(ro->element);
    return 0;
}

static PyObject *
repeat_next(repeatobject *ro)
{
    if (ro->cnt == 0)
        return NULL;
    if (ro->cnt > 0)
        ro->cnt--;
    Py_INCREF(ro->element);
    return ro->element;
}

static PyObject *
repeat_repr(repeatobject *ro)
{
    if (ro->cnt == -1)
        return PyUnicode_FromFormat("repeat(%R)", ro->element);
    else
        return PyUnicode_FromFormat("repeat(%R, %zd)", ro->element, ro->cnt);
}

static PyObject *
repeat_len(repeatobject *ro)
{
    if (ro->cnt == -1) {
        PyErr_SetString(PyExc_TypeError, "len() of unsized object");
        return NULL;
    }
    return PyLong_FromSize_t(ro->cnt);
}

PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");

static PyObject *
repeat_reduce(repeatobject *ro)
{
    /* unpickle this so that a new repeat iterator is constructed with an
     * object, then call __setstate__ on it to set cnt
     */
    if (ro->cnt >= 0)
        return Py_BuildValue("O(On)", Py_TYPE(ro), ro->element, ro->cnt);
    else
        return Py_BuildValue("O(O)", Py_TYPE(ro), ro->element);
}

static PyMethodDef repeat_methods[] = {
    {"__length_hint__", (PyCFunction)repeat_len, METH_NOARGS, length_hint_doc},
    {"__reduce__",      (PyCFunction)repeat_reduce, METH_NOARGS, reduce_doc},
    {NULL,              NULL}           /* sentinel */
};

PyDoc_STRVAR(repeat_doc,
"repeat(object [,times]) -> create an iterator which returns the object\n\
for the specified number of times.  If not specified, returns the object\n\
endlessly.");

static PyTypeObject repeat_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.repeat",                 /* tp_name */
    sizeof(repeatobject),               /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)repeat_dealloc,         /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    (reprfunc)repeat_repr,              /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    repeat_doc,                         /* tp_doc */
    (traverseproc)repeat_traverse,      /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)repeat_next,          /* tp_iternext */
    repeat_methods,                     /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    repeat_new,                         /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};

/* ziplongest object *********************************************************/

typedef struct {
    PyObject_HEAD
    Py_ssize_t tuplesize;
    Py_ssize_t numactive;
    PyObject *ittuple;                  /* tuple of iterators */
    PyObject *result;
    PyObject *fillvalue;
} ziplongestobject;

static PyTypeObject ziplongest_type;

static PyObject *
zip_longest_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    ziplongestobject *lz;
    Py_ssize_t i;
    PyObject *ittuple;  /* tuple of iterators */
    PyObject *result;
    PyObject *fillvalue = Py_None;
    Py_ssize_t tuplesize = PySequence_Length(args);

    if (kwds != NULL && PyDict_CheckExact(kwds) && PyDict_Size(kwds) > 0) {
        fillvalue = PyDict_GetItemString(kwds, "fillvalue");
        if (fillvalue == NULL  ||  PyDict_Size(kwds) > 1) {
            PyErr_SetString(PyExc_TypeError,
                "zip_longest() got an unexpected keyword argument");
            return NULL;
        }
    }

    /* args must be a tuple */
    assert(PyTuple_Check(args));

    /* obtain iterators */
    ittuple = PyTuple_New(tuplesize);
    if (ittuple == NULL)
        return NULL;
    for (i=0; i < tuplesize; i++) {
        PyObject *item = PyTuple_GET_ITEM(args, i);
        PyObject *it = PyObject_GetIter(item);
        if (it == NULL) {
            if (PyErr_ExceptionMatches(PyExc_TypeError))
                PyErr_Format(PyExc_TypeError,
                    "zip_longest argument #%zd must support iteration",
                    i+1);
            Py_DECREF(ittuple);
            return NULL;
        }
        PyTuple_SET_ITEM(ittuple, i, it);
    }

    /* create a result holder */
    result = PyTuple_New(tuplesize);
    if (result == NULL) {
        Py_DECREF(ittuple);
        return NULL;
    }
    for (i=0 ; i < tuplesize ; i++) {
        Py_INCREF(Py_None);
        PyTuple_SET_ITEM(result, i, Py_None);
    }

    /* create ziplongestobject structure */
    lz = (ziplongestobject *)type->tp_alloc(type, 0);
    if (lz == NULL) {
        Py_DECREF(ittuple);
        Py_DECREF(result);
        return NULL;
    }
    lz->ittuple = ittuple;
    lz->tuplesize = tuplesize;
    lz->numactive = tuplesize;
    lz->result = result;
    Py_INCREF(fillvalue);
    lz->fillvalue = fillvalue;
    return (PyObject *)lz;
}

static void
zip_longest_dealloc(ziplongestobject *lz)
{
    PyObject_GC_UnTrack(lz);
    Py_XDECREF(lz->ittuple);
    Py_XDECREF(lz->result);
    Py_XDECREF(lz->fillvalue);
    Py_TYPE(lz)->tp_free(lz);
}

static int
zip_longest_traverse(ziplongestobject *lz, visitproc visit, void *arg)
{
    Py_VISIT(lz->ittuple);
    Py_VISIT(lz->result);
    Py_VISIT(lz->fillvalue);
    return 0;
}

static PyObject *
zip_longest_next(ziplongestobject *lz)
{
    Py_ssize_t i;
    Py_ssize_t tuplesize = lz->tuplesize;
    PyObject *result = lz->result;
    PyObject *it;
    PyObject *item;
    PyObject *olditem;

    if (tuplesize == 0)
        return NULL;
    if (lz->numactive == 0)
        return NULL;
    if (Py_REFCNT(result) == 1) {
        Py_INCREF(result);
        for (i=0 ; i < tuplesize ; i++) {
            it = PyTuple_GET_ITEM(lz->ittuple, i);
            if (it == NULL) {
                Py_INCREF(lz->fillvalue);
                item = lz->fillvalue;
            } else {
                item = PyIter_Next(it);
                if (item == NULL) {
                    lz->numactive -= 1;
                    if (lz->numactive == 0 || PyErr_Occurred()) {
                        lz->numactive = 0;
                        Py_DECREF(result);
                        return NULL;
                    } else {
                        Py_INCREF(lz->fillvalue);
                        item = lz->fillvalue;
                        PyTuple_SET_ITEM(lz->ittuple, i, NULL);
                        Py_DECREF(it);
                    }
                }
            }
            olditem = PyTuple_GET_ITEM(result, i);
            PyTuple_SET_ITEM(result, i, item);
            Py_DECREF(olditem);
        }
    } else {
        result = PyTuple_New(tuplesize);
        if (result == NULL)
            return NULL;
        for (i=0 ; i < tuplesize ; i++) {
            it = PyTuple_GET_ITEM(lz->ittuple, i);
            if (it == NULL) {
                Py_INCREF(lz->fillvalue);
                item = lz->fillvalue;
            } else {
                item = PyIter_Next(it);
                if (item == NULL) {
                    lz->numactive -= 1;
                    if (lz->numactive == 0 || PyErr_Occurred()) {
                        lz->numactive = 0;
                        Py_DECREF(result);
                        return NULL;
                    } else {
                        Py_INCREF(lz->fillvalue);
                        item = lz->fillvalue;
                        PyTuple_SET_ITEM(lz->ittuple, i, NULL);
                        Py_DECREF(it);
                    }
                }
            }
            PyTuple_SET_ITEM(result, i, item);
        }
    }
    return result;
}

static PyObject *
zip_longest_reduce(ziplongestobject *lz)
{

    /* Create a new tuple with empty sequences where appropriate to pickle.
     * Then use setstate to set the fillvalue
     */
    int i;
    PyObject *args = PyTuple_New(PyTuple_GET_SIZE(lz->ittuple));

    if (args == NULL)
        return NULL;
    for (i=0; i<PyTuple_GET_SIZE(lz->ittuple); i++) {
        PyObject *elem = PyTuple_GET_ITEM(lz->ittuple, i);
        if (elem == NULL) {
            elem = PyTuple_New(0);
            if (elem == NULL) {
                Py_DECREF(args);
                return NULL;
            }
        } else
            Py_INCREF(elem);
        PyTuple_SET_ITEM(args, i, elem);
    }
    return Py_BuildValue("ONO", Py_TYPE(lz), args, lz->fillvalue);
}

static PyObject *
zip_longest_setstate(ziplongestobject *lz, PyObject *state)
{
    Py_INCREF(state);
    Py_XSETREF(lz->fillvalue, state);
    Py_RETURN_NONE;
}

static PyMethodDef zip_longest_methods[] = {
    {"__reduce__",      (PyCFunction)zip_longest_reduce,      METH_NOARGS,
     reduce_doc},
    {"__setstate__",    (PyCFunction)zip_longest_setstate,    METH_O,
     setstate_doc},
    {NULL,              NULL}   /* sentinel */
};

PyDoc_STRVAR(zip_longest_doc,
"zip_longest(iter1 [,iter2 [...]], [fillvalue=None]) --> zip_longest object\n\
\n\
Return a zip_longest object whose .__next__() method returns a tuple where\n\
the i-th element comes from the i-th iterable argument.  The .__next__()\n\
method continues until the longest iterable in the argument sequence\n\
is exhausted and then it raises StopIteration.  When the shorter iterables\n\
are exhausted, the fillvalue is substituted in their place.  The fillvalue\n\
defaults to None or can be specified by a keyword argument.\n\
");

static PyTypeObject ziplongest_type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "itertools.zip_longest",            /* tp_name */
    sizeof(ziplongestobject),           /* tp_basicsize */
    0,                                  /* tp_itemsize */
    /* methods */
    (destructor)zip_longest_dealloc,    /* tp_dealloc */
    0,                                  /* tp_print */
    0,                                  /* tp_getattr */
    0,                                  /* tp_setattr */
    0,                                  /* tp_reserved */
    0,                                  /* tp_repr */
    0,                                  /* tp_as_number */
    0,                                  /* tp_as_sequence */
    0,                                  /* tp_as_mapping */
    0,                                  /* tp_hash */
    0,                                  /* tp_call */
    0,                                  /* tp_str */
    PyObject_GenericGetAttr,            /* tp_getattro */
    0,                                  /* tp_setattro */
    0,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
        Py_TPFLAGS_BASETYPE,            /* tp_flags */
    zip_longest_doc,                    /* tp_doc */
    (traverseproc)zip_longest_traverse, /* tp_traverse */
    0,                                  /* tp_clear */
    0,                                  /* tp_richcompare */
    0,                                  /* tp_weaklistoffset */
    PyObject_SelfIter,                  /* tp_iter */
    (iternextfunc)zip_longest_next,     /* tp_iternext */
    zip_longest_methods,                /* tp_methods */
    0,                                  /* tp_members */
    0,                                  /* tp_getset */
    0,                                  /* tp_base */
    0,                                  /* tp_dict */
    0,                                  /* tp_descr_get */
    0,                                  /* tp_descr_set */
    0,                                  /* tp_dictoffset */
    0,                                  /* tp_init */
    0,                                  /* tp_alloc */
    zip_longest_new,                    /* tp_new */
    PyObject_GC_Del,                    /* tp_free */
};

/* module level code ********************************************************/

PyDoc_STRVAR(module_doc,
"Functional tools for creating and using iterators.\n\
\n\
Infinite iterators:\n\
count(start=0, step=1) --> start, start+step, start+2*step, ...\n\
cycle(p) --> p0, p1, ... plast, p0, p1, ...\n\
repeat(elem [,n]) --> elem, elem, elem, ... endlessly or up to n times\n\
\n\
Iterators terminating on the shortest input sequence:\n\
accumulate(p[, func]) --> p0, p0+p1, p0+p1+p2\n\
chain(p, q, ...) --> p0, p1, ... plast, q0, q1, ... \n\
chain.from_iterable([p, q, ...]) --> p0, p1, ... plast, q0, q1, ... \n\
compress(data, selectors) --> (d[0] if s[0]), (d[1] if s[1]), ...\n\
dropwhile(pred, seq) --> seq[n], seq[n+1], starting when pred fails\n\
groupby(iterable[, keyfunc]) --> sub-iterators grouped by value of keyfunc(v)\n\
filterfalse(pred, seq) --> elements of seq where pred(elem) is False\n\
islice(seq, [start,] stop [, step]) --> elements from\n\
       seq[start:stop:step]\n\
starmap(fun, seq) --> fun(*seq[0]), fun(*seq[1]), ...\n\
tee(it, n=2) --> (it1, it2 , ... itn) splits one iterator into n\n\
takewhile(pred, seq) --> seq[0], seq[1], until pred fails\n\
zip_longest(p, q, ...) --> (p[0], q[0]), (p[1], q[1]), ... \n\
\n\
Combinatoric generators:\n\
product(p, q, ... [repeat=1]) --> cartesian product\n\
permutations(p[, r])\n\
combinations(p, r)\n\
combinations_with_replacement(p, r)\n\
");


static PyMethodDef module_methods[] = {
    {"tee",     (PyCFunction)tee,       METH_VARARGS, tee_doc},
    {NULL,              NULL}           /* sentinel */
};


static struct PyModuleDef itertoolsmodule = {
    PyModuleDef_HEAD_INIT,
    "itertools",
    module_doc,
    -1,
    module_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit_itertools(void)
{
    int i;
    PyObject *m;
    char *name;
    PyTypeObject *typelist[] = {
        &accumulate_type,
        &combinations_type,
        &cwr_type,
        &cycle_type,
        &dropwhile_type,
        &takewhile_type,
        &islice_type,
        &starmap_type,
        &chain_type,
        &compress_type,
        &filterfalse_type,
        &count_type,
        &ziplongest_type,
        &permutations_type,
        &product_type,
        &repeat_type,
        &groupby_type,
        &_grouper_type,
        &tee_type,
        &teedataobject_type,
        NULL
    };

    Py_TYPE(&teedataobject_type) = &PyType_Type;
    m = PyModule_Create(&itertoolsmodule);
    if (m == NULL)
        return NULL;

    for (i=0 ; typelist[i] != NULL ; i++) {
        if (PyType_Ready(typelist[i]) < 0)
            return NULL;
        name = strchr(typelist[i]->tp_name, '.');
        assert (name != NULL);
        Py_INCREF(typelist[i]);
        PyModule_AddObject(m, name+1, (PyObject *)typelist[i]);
    }

    return m;
}
